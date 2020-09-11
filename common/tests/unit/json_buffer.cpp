/** Unit tests for buffer
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

#include <cstdlib>
#include <ctime>

#include <criterion/criterion.h>
#include <jansson.h>

#include <villas/json_buffer.hpp>

using namespace villas;

using villas::JsonBuffer;

// cppcheck-suppress unknownMacro
TestSuite(buffer, .description = "Buffer datastructure");

Test(json_buffer, decode)
{
	JsonBuffer buf;
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

Test(json_buffer, encode)
{
	int ret;
	JsonBuffer buf;
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

Test(json_buffer, encode_decode)
{
	int ret;
	JsonBuffer buf;
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

Test(json_buffer, multiple)
{
	int ret;
	const int N = 100;

	JsonBuffer buf;
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
