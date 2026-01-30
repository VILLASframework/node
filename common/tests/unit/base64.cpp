/* Unit tests for base64 encoding/decoding.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <criterion/criterion.h>

#include <villas/utils.hpp>

using namespace villas::utils::base64;

// cppcheck-suppress unknownMacro
TestSuite(base64, .description = "Base64 En/decoder");

static std::vector<std::byte> vec(const char *str) {
  auto bytes = std::as_bytes(std::span{str, strlen(str)});
  return std::vector<std::byte>(bytes.begin(), bytes.end());
}

Test(base64, encoding) {
  cr_assert(encode(vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole")) ==
            "cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=");
}

Test(base64, decoding) {
  cr_assert(decode("cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=") ==
            vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole"));
}
