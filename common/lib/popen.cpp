/* Bi-directional popen.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>

#include <fmt/core.h>
#include <villas/exceptions.hpp>
#include <villas/popen.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace utils {

Popen::Popen(const std::string &cmd, const arg_list &args, const env_map &env,
             const std::string &wd, bool sh)
    : command(cmd), working_dir(wd), arguments(args), environment(env),
      shell(sh) {
  open();
}

Popen::~Popen() { close(); }

void Popen::kill(int signal) { ::kill(pid, signal); }

void Popen::open() {
  std::vector<char *> argv, envp;

  const int READ = 0;
  const int WRITE = 1;

  int ret;
  int inpipe[2];
  int outpipe[2];

  ret = pipe(inpipe);
  if (ret)
    goto error_out;

  ret = pipe(outpipe);
  if (ret)
    goto clean_inpipe_out;

  fd_in = outpipe[READ];
  fd_out = inpipe[WRITE];

  pid = fork();
  if (pid == -1)
    goto clean_outpipe_out;

  if (pid == 0) {
    // Prepare arguments
    if (shell) {
      argv.push_back((char *)"sh");
      argv.push_back((char *)"-c");
      argv.push_back((char *)command.c_str());
    } else {
      for (auto const &arg : arguments) {
        auto s = strdup(arg.c_str());

        argv.push_back(s);
      }
    }

    // Prepare environment
    for (char **p = environ; *p; p++)
      envp.push_back(*p);

    for (auto env : environment) {
      auto e = fmt::format("{}={}", env.first, env.second);
      auto s = strdup(e.c_str());

      envp.push_back(s);
    }

    argv.push_back(nullptr);
    envp.push_back(nullptr);

    // Redirect IO
    ::close(outpipe[READ]);
    ::close(inpipe[WRITE]);

    if (inpipe[READ] != STDIN_FILENO) {
      dup2(inpipe[READ], STDIN_FILENO);
      ::close(inpipe[READ]);
    }

    if (outpipe[WRITE] != STDOUT_FILENO) {
      dup2(outpipe[WRITE], STDOUT_FILENO);
      ::close(outpipe[WRITE]);
    }

    // Change working directory
    if (!working_dir.empty()) {
      int ret;

      ret = chdir(working_dir.c_str());
      if (ret)
        exit(127);
    }

    execvpe(shell ? _PATH_BSHELL : command.c_str(), (char *const *)argv.data(),
            (char *const *)envp.data());
    exit(127);
  }

  ::close(outpipe[WRITE]);
  ::close(inpipe[READ]);

  return;

clean_outpipe_out:
  ::close(outpipe[READ]);
  ::close(outpipe[WRITE]);

clean_inpipe_out:
  ::close(inpipe[READ]);
  ::close(inpipe[WRITE]);

error_out:
  throw SystemError("Failed to start subprocess");
}

int Popen::close() {
  int pstat;

  if (pid != -1) {
    do {
      pid = waitpid(pid, &pstat, 0);
    } while (pid == -1 && errno == EINTR);
  }

  return pid == -1 ? -1 : pstat;
}

PopenStream::PopenStream(const std::string &cmd, const arg_list &args,
                         const env_map &env, const std::string &wd, bool sh)
    : Popen(cmd, args, env, wd, sh) {
  open();
}

PopenStream::~PopenStream() { close(); }

void PopenStream::open() {
  Popen::open();

  input.buffer = std::make_unique<stdio_buf>(fd_in, std::ios_base::in);
  output.buffer = std::make_unique<stdio_buf>(fd_out, std::ios_base::out);

  input.stream = std::make_unique<std::istream>(input.buffer.get());
  output.stream = std::make_unique<std::ostream>(output.buffer.get());
}

int PopenStream::close() {
  int ret = Popen::close();

  input.stream.reset();
  output.stream.reset();

  input.buffer.reset();
  output.buffer.reset();

  return ret;
}

} // namespace utils
} // namespace villas
