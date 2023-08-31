/* Bi-directional popen.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <signal.h>

#include <ext/stdio_filebuf.h>

#include <map>
#include <string>
#include <istream>
#include <ostream>
#include <memory>

namespace villas {
namespace utils {

class Popen {

public:
	using arg_list = std::vector<std::string>;
	using env_map = std::map<std::string, std::string>;

	using char_type = char;
	using stdio_buf = __gnu_cxx::stdio_filebuf<char_type>;

	Popen(const std::string &cmd,
	      const arg_list &args = arg_list(),
	      const env_map &env = env_map(),
	      const std::string &wd = std::string(),
	      bool shell = false);
	~Popen();

	void open();
	int close();
	void kill(int signal = SIGINT);

	int getFdIn()
	{
		return fd_in;
	}

	int getFdOut()
	{
		return fd_out;
	}

	pid_t getPid() const
	{
		return pid;
	}

protected:
	std::string command;
	std::string working_dir;
	arg_list arguments;
	env_map environment;
	bool shell;
	pid_t pid;

	int fd_in, fd_out;
};

class PopenStream : public Popen {

public:
	PopenStream(const std::string &cmd,
	      const arg_list &args = arg_list(),
	      const env_map &env = env_map(),
	      const std::string &wd = std::string(),
	      bool shell = false);
	~PopenStream();

	std::istream &cin()
	{
		return *(input.stream);
	}

	std::ostream &cout()
	{
		return *(output.stream);
	}

	void open();
	int close();

protected:

	struct {
		std::unique_ptr<std::istream> stream;
		std::unique_ptr<stdio_buf> buffer;
	} input;

	struct {
		std::unique_ptr<std::ostream> stream;
		std::unique_ptr<stdio_buf> buffer;
	} output;
};

} // namespace utils
} // namespace villas

template<typename T>
std::istream &operator>>(villas::utils::PopenStream &po, T &value)
{
	return *(po.input.stream) >> value;
}
