/** Version.
 *
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdexcept>
#include <string>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/version.hpp>

using namespace villas::utils;

Version::Version(const std::string &str)
{
	size_t endpos;

	auto comp = tokenize(str, ".");

	if (comp.size() > 3)
		throw std::invalid_argument("Not a valid version string");

	for (unsigned i = 0; i < 3; i++) {
		if (i < comp.size()) {
			components[i] = std::stoi(comp[i], &endpos, 10);

			if (comp[i].begin() + endpos != comp[i].end())
				throw std::invalid_argument("Not a valid version string");
		}
		else
			components[i] = 0;
	}
}

Version::Version(int maj, int min, int pat) :
	components{maj, min, pat}
{

}

int Version::cmp(const Version& lhs, const Version& rhs)
{
	int d;

	for (int i = 0; i < 3; i++) {
		d = lhs.components[i] - rhs.components[i];
		if (d)
			return d;
	}

	return 0;
}
