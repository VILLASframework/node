/* Unit tests for buffer
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#include <cstdlib>
#include <ctime>

#include <criterion/criterion.h>
#include <jansson.h>

#include <villas/buffer.hpp>

using namespace villas;

// cppcheck-suppress unknownMacro
TestSuite(buffer, .description = "Buffer datastructure");

Test(buffer, decode)
{
	Buffer buf;
	json_t *j;
	json_t *k;

	const char *e = "{\"id\": \"5a786626-fbc6-4c04-98c2-48027e68c2fa\"}";
	size_t len = strlen(e);

	buf.insert(buf.begin(), e, e+len);

	k = json_loads(e, 0, nullptr);
	cr_assert_not_null(k);

	j = buf.decode();
	cr_assert_not_null(j);
	cr_assert(json_equal(j, k));
}

Test(buffer, encode)
{
	int ret;
	Buffer buf;
	json_t *k;

	const char *e = "{\"id\": \"5a786626-fbc6-4c04-98c2-48027e68c2fa\"}";

	k = json_loads(e, 0, nullptr);
	cr_assert_not_null(k);

	ret = buf.encode(k);
	cr_assert_eq(ret, 0);

	char *f = buf.data();
	cr_assert_not_null(f);

	cr_assert_str_eq(e, f);

	json_decref(k);
}

Test(buffer, encode_decode)
{
	int ret;
	Buffer buf;
	json_t *k;
	json_t *j;

	const char *e = "{\"id\": \"5a786626-fbc6-4c04-98c2-48027e68c2fa\"}";

	k = json_loads(e, 0, nullptr);
	cr_assert_not_null(k);

	ret = buf.encode(k);
	cr_assert_eq(ret, 0);

	j = buf.decode();
	cr_assert_not_null(j);

	cr_assert(json_equal(j, k));

	json_decref(j);
	json_decref(k);
}

Test(buffer, multiple)
{
	int ret;
	const int N = 100;

	Buffer buf;
	json_t *k[N];
	json_t *j[N];

	std::srand(std::time(nullptr));

	for (int i = 0; i < N; i++) {
		k[i] = json_pack("{ s: i }", "id", std::rand());
		cr_assert_not_null(k[i]);

		ret = buf.encode(k[i]);
		cr_assert_eq(ret, 0);
	}

	for (int i = 0; i < N; i++) {
		j[i] = buf.decode();
		cr_assert_not_null(j[i]);

		cr_assert(json_equal(k[i], j[i]));

		json_decref(k[i]);
		json_decref(j[i]);
	}
}
