/** Utilities.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <string>
#include <vector>

#include <signal.h>

namespace villas {
namespace utils {

std::vector<std::string>
tokenize(std::string s, std::string delimiter);


template<typename T>
void
assertExcept(bool condition, const T& exception)
{
	if(not condition)
		throw exception;
}

/** Register a exit callback for program termination: SIGINT, SIGKILL & SIGALRM. */
int signals_init(void (*cb)(int signal, siginfo_t *sinfo, void *ctx));

/** Fill buffer with random data */
ssize_t read_random(char *buf, size_t len);

/** Remove ANSI control sequences for colored output. */
char * decolor(char *str);

} // namespace utils
} // namespace villas

