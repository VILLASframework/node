/** Human readable cpusets.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/cpuset.hpp>
#include <villas/utils.hpp>

using namespace villas::utils;

#ifdef __linux__

CpuSet::CpuSet(uintmax_t iset) :
	CpuSet()
{
	zero();

	for (size_t i = 0; i < num_cpus; i++) {
		if (iset & (1L << i))
			set(i);
	}
}

CpuSet::CpuSet(const std::string &str) :
	CpuSet()
{
	size_t endpos, start, end;

	for (auto token : tokenize(str, ",")) {
		auto sep = token.find('-');

		if (sep == std::string::npos) {
			start = std::stoi(token, &endpos);

			if (token.begin() + endpos != token.end())
				throw std::invalid_argument("Not a valid CPU set");

			if (start < num_cpus)
				set(start);
		}
		else {
			start = std::stoi(token, &endpos);

			if (token.begin() + endpos != token.begin() + sep)
				throw std::invalid_argument("Not a valid CPU set");

			auto token2 = token.substr(endpos + 1);

			end = std::stoi(token2, &endpos);

			if (token2.begin() + endpos != token2.end())
				throw std::invalid_argument("Not a valid CPU set");

			for (size_t i = start; i <= end && i < num_cpus; i++)
				set(i);
		}
	}
}

CpuSet::CpuSet(const char *str)
	: CpuSet(std::string(str))
{ }

CpuSet::operator uintmax_t()
{
	uintmax_t iset = 0;

	for (size_t i = 0; i < num_cpus; i++) {
		if (isSet(i))
			iset |= 1ULL << i;
	}

	return iset;
}

CpuSet::operator std::string ()
{
	std::stringstream ss;

	bool first = true;

	for (size_t i = 0; i < num_cpus; i++) {
		if (isSet(i)) {
			size_t run = 0;
			for (size_t j = i + 1; j < num_cpus; j++) {
				if (!isSet(j))
					break;

				run++;
			}

			if (first)
				first = false;
			else
				ss << ",";

			ss << i;

			if (run == 1) {
				ss << "," << (i + 1);
				i++;
			}
			else if (run > 1) {
				ss << "-" << (i + run);
				i += run;
			}
		}
	}

	return ss.str();
}

#endif /* __linux__ */
