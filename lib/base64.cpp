/* Base64 encoding/decoding.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <villas/utils.hpp>

namespace villas {
namespace utils {
namespace base64 {

static const char kEncodeLookup[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char kPadCharacter = '=';

std::string encode(std::span<const std::byte> input) {
  std::string encoded;
  encoded.reserve(((input.size() / 3) + (input.size() % 3 > 0)) * 4);

  auto it = input.begin();

  for (std::size_t i = 0; i < input.size() / 3; ++i) {
    auto temp = std::to_integer<std::uint32_t>(*it++) << 16;
    temp += std::to_integer<std::uint32_t>(*it++) << 8;
    temp += std::to_integer<std::uint32_t>(*it++);
    encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
    encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
    encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6]);
    encoded.append(1, kEncodeLookup[(temp & 0x0000003F)]);
  }

  switch (input.size() % 3) {
  case 1: {
    auto temp = std::to_integer<std::uint32_t>(*it++) << 16;
    encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
    encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
    encoded.append(2, kPadCharacter);
  } break;

  case 2: {
    auto temp = std::to_integer<std::uint32_t>(*it++) << 16;
    temp += std::to_integer<std::uint32_t>(*it++) << 8;
    encoded.append(1, kEncodeLookup[(temp & 0x00FC0000) >> 18]);
    encoded.append(1, kEncodeLookup[(temp & 0x0003F000) >> 12]);
    encoded.append(1, kEncodeLookup[(temp & 0x00000FC0) >> 6]);
    encoded.append(1, kPadCharacter);
  } break;
  }

  return encoded;
}

std::vector<std::byte> decode(std::string_view input) {
  if (input.length() % 4)
    throw std::runtime_error("Invalid base64 length!");

  std::size_t padding{};

  if (input.length()) {
    if (input[input.length() - 1] == kPadCharacter)
      padding++;
    if (input[input.length() - 2] == kPadCharacter)
      padding++;
  }

  std::vector<std::byte> decoded;
  decoded.reserve(((input.length() / 4) * 3) - padding);

  std::uint32_t temp{};
  auto it = input.begin();

  while (it < input.end()) {
    for (std::size_t i = 0; i < 4; ++i) {
      temp <<= 6;
      if (*it >= 0x41 && *it <= 0x5A)
        temp |= *it - 0x41;
      else if (*it >= 0x61 && *it <= 0x7A)
        temp |= *it - 0x47;
      else if (*it >= 0x30 && *it <= 0x39)
        temp |= *it + 0x04;
      else if (*it == 0x2B)
        temp |= 0x3E;
      else if (*it == 0x2F)
        temp |= 0x3F;
      else if (*it == kPadCharacter) {
        switch (input.end() - it) {
        case 1:
          decoded.push_back(static_cast<std::byte>((temp >> 16) & 0x000000FF));
          decoded.push_back(static_cast<std::byte>((temp >> 8) & 0x000000FF));
          return decoded;
        case 2:
          decoded.push_back(static_cast<std::byte>((temp >> 10) & 0x000000FF));
          return decoded;
        default:
          throw std::runtime_error("Invalid padding in base64!");
        }
      } else
        throw std::runtime_error("Invalid character in base64!");

      ++it;
    }

    decoded.push_back(static_cast<std::byte>((temp >> 16) & 0x000000FF));
    decoded.push_back(static_cast<std::byte>((temp >> 8) & 0x000000FF));
    decoded.push_back(static_cast<std::byte>((temp) & 0x000000FF));
  }

  return decoded;
}

} // namespace base64
} // namespace utils
} // namespace villas
