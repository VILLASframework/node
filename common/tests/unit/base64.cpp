/* Unit tests for base64 encoding/decoding
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#include <criterion/criterion.h>

#include <iostream>

#include <villas/utils.hpp>

using namespace villas::utils::base64;

// cppcheck-suppress unknownMacro
TestSuite(base64, .description = "Base64 En/decoder");

static
std::vector<byte> vec(const char *str)
{
	return std::vector<byte>((byte *) str, (byte *) str + strlen(str));
}

Test(base64, encoding)
{
	cr_assert(encode(vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole")) == "cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=");
}

Test(base64, decoding)
{
	cr_assert(decode("cG9oeTBBaXkxWmFWYTVhaWsyeWFpeTNpZm9oM29vbGU=") == vec("pohy0Aiy1ZaVa5aik2yaiy3ifoh3oole"));
}
