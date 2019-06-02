/** Bi-directional popen
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <dirent.h>

#include <villas/exceptions.hpp>
#include <villas/popen.hpp>

namespace villas {
namespace utils {

Popen::Popen(const std::string &cmd) :
	command(cmd)
{
	open();
}

Popen::~Popen()
{
	close();
}

void Popen::kill(int signal)
{
	::kill(pid, signal);
}

void Popen::open()
{
	const int READ = 0;
	const int WRITE = 1;

	int ret;
	int inpipe[2];
	int outpipe[2];
	char *argv[4];

	ret = pipe(inpipe);
	if (ret)
		goto error_out;

	ret = pipe(outpipe);
	if (ret)
		goto clean_inpipe_out;

	input.buffer = std::make_unique<stdio_buf>(outpipe[READ], std::ios_base::in);
	output.buffer = std::make_unique<stdio_buf>(inpipe[WRITE], std::ios_base::out);

	input.stream  = std::make_unique<std::istream>(input.buffer.get());
	output.stream = std::make_unique<std::ostream>(output.buffer.get());

	pid = fork();
	if (pid == -1)
		goto clean_outpipe_out;

	if (pid == 0) {
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

		argv[0] = (char *) "sh";
		argv[1] = (char *) "-c";
		argv[2] = (char *) command.c_str();
		argv[3] = nullptr;

		execv(_PATH_BSHELL, (char * const *) argv);
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

int Popen::close()
{
	int pstat;

	if (pid != -1) {
		do {
			pid = waitpid(pid, &pstat, 0);
		} while (pid == -1 && errno == EINTR);
	}

	input.stream.reset();
	output.stream.reset();

	input.buffer.reset();
	output.buffer.reset();

	return pid == -1 ? -1 : pstat;
}

} /* namespace utils */
} /* namespace villas */
