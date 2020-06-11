/** Base64 encoding/decoding
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>

#include <villas/utils.hpp>

namespace villas {
namespace utils {
namespace base64 {

static const char kEncodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char kPadCharacter = '=';

std::string encode(const std::vector<byte>& input)
{
	std::string encoded;
	encoded.reserve(((input.size() / 3) + (input.size() % 3 > 0)) * 4);

	std::uint32_t temp{};
	auto it = input.begin();

	for (std::size_t i = 0; i < input.size() / 3; ++i) {
		temp  = (*it++) << 16;
		temp += (*it++) << 8;
		temp += (*it++);
		encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
		encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
		encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6 ]);
		encoded.append(1, kEncodeLookup[(temp & 0x0000003F)      ]);
	}

	switch (input.size() % 3) {
		case 1:
			temp = (*it++) << 16;
			encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
			encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
			encoded.append(2, kPadCharacter);
			break;

		case 2:
			temp  = (*it++) << 16;
			temp += (*it++) << 8;
			encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
			encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
			encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6 ]);
			encoded.append(1, kPadCharacter);
			break;
	}

	return encoded;
}

std::vector<byte> decode(const std::string& input)
{
	if (input.length() % 4)
		throw std::runtime_error("Invalid base64 length!");

	std::size_t padding{};

	if (input.length()) {
		if (input[input.length() - 1] == kPadCharacter) padding++;
		if (input[input.length() - 2] == kPadCharacter) padding++;
	}

	std::vector<byte> decoded;
	decoded.reserve(((input.length() / 4) * 3) - padding);

	std::uint32_t temp{};
	auto it = input.begin();

	while (it < input.end()) {
		for (std::size_t i = 0; i < 4; ++i) {
			temp <<= 6;
			if      (*it >= 0x41 && *it <= 0x5A) temp |= *it - 0x41;
			else if (*it >= 0x61 && *it <= 0x7A) temp |= *it - 0x47;
			else if (*it >= 0x30 && *it <= 0x39) temp |= *it + 0x04;
			else if (*it == 0x2B)                temp |= 0x3E;
			else if (*it == 0x2F)                temp |= 0x3F;
			else if (*it == kPadCharacter) {
				switch(input.end() - it) {
					case 1:
						decoded.push_back((temp >> 16) & 0x000000FF);
						decoded.push_back((temp >> 8 ) & 0x000000FF);
						return decoded;
					case 2:
						decoded.push_back((temp >> 10) & 0x000000FF);
						return decoded;
					default:
						throw std::runtime_error("Invalid padding in base64!");
				}
			}
			else
				throw std::runtime_error("Invalid character in base64!");

			++it;
		}

		decoded.push_back((temp >> 16) & 0x000000FF);
		decoded.push_back((temp >> 8 ) & 0x000000FF);
		decoded.push_back((temp      ) & 0x000000FF);
	}

	return decoded;
}

} /* namespace base64 */
} /* namespace utils */
} /* namespace villas */
