/** Unit tests for base64 encoding/decoding
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>

#include <iostream>

#include <villas/utils.hpp>

using namespace villas::utils::base64;

// cppcheck-suppress unknownMacro
TestSuite(base64, .description = "Base64 En/decoder");

static std::vector<byte> vec(const char *str)
{
	return std::vector<byte>((byte *) str, (byte *) str + strlen(str));
}

// static std::string str(const std::vector<byte> &vec)
// {
// 	return std::string((char *) vec.data(), vec.size());
// }

Test(base64, encoding)
{
	cr_assert(encode(vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole")) == "cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=");
}

Test(base64, decoding)
{
	cr_assert(decode("cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=") == vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole"));
}
