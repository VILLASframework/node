/** Bi-directional popen
 *
 * @file
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

	std::istream &cin()
	{
		return *(input.stream);
	}

	std::ostream &cout()
	{
		return *(output.stream);
	}

	int getFd()
	{
		return input.buffer->fd();
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

	struct {
		std::unique_ptr<std::istream> stream;
		std::unique_ptr<stdio_buf> buffer;
	} input;

	struct {
		std::unique_ptr<std::ostream> stream;
		std::unique_ptr<stdio_buf> buffer;
	} output;
};

} /* namespace utils */
} /* namespace villas */

template<typename T>
std::istream& operator>>(villas::utils::Popen& po, T& value)
{
	return *(po.input.stream) >> value;
}
