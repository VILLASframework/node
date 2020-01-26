/** Unit tests for libjansson helpers
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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
#include <criterion/parameterized.h>

#include <villas/utils.hpp>
#include <villas/config_helper.hpp>

#include "helpers.hpp"

struct param {
	char *argv[32];
	char *json;
};

ParameterizedTestParameters(json, json_load_cli)
{
	const auto d = cr_strdup;

	static criterion::parameters<struct param> params = {
		// Combined long option
		{
			.argv = { d("dummy"), d("--option=value") },
			.json = d("{ \"option\" : \"value\" }")
		},
		// Separated long option
		{
			.argv = { d("dummy"), d("--option"), d("value") },
			.json = d("{ \"option\" : \"value\" }")
		},
		// All kinds of data types
		{
			.argv = { d("dummy"), d("--integer"), d("1"), d("--real"), d("1.1"), d("--bool"), d("true"), d("--null"), d("null"), d("--string"), d("hello world") },
			.json = d("{ \"integer\" : 1, \"real\" : 1.1, \"bool\" : true, \"null\" : null, \"string\" : \"hello world\" }")
		},
		// Array generation
		{
			.argv = { d("dummy"), d("--bool"), d("true"), d("--bool"), d("false") },
			.json = d("{ \"bool\" : [ true, false ] }")
		},
		// Dots in the option name generate sub objects
		{
			.argv = { d("dummy"), d("--sub.option"), d("value") },
			.json = d("{ \"sub\" : { \"option\" : \"value\" } }")
		},
		// Nesting is possible
		{
			.argv = { d("dummy"), d("--sub.sub.option"), d("value") },
			.json = d("{ \"sub\" : { \"sub\" : { \"option\" : \"value\" } } }")
		},
		// Multiple subgroups are merged
		{
			.argv = { d("dummy"), d("--sub.sub.option"), d("value1"), d("--sub.option"), d("value2") },
			.json = d("{ \"sub\" : { \"option\" : \"value2\", \"sub\" : { \"option\" : \"value1\" } } }")
		}
	};

	return params;
}

ParameterizedTest(struct param *p, json, json_load_cli)
{
	json_error_t err;
	json_t *json, *cli;

	json = json_loads(p->json, 0, &err);
	cr_assert_not_null(json);

	int argc = 0;
	while (p->argv[argc]) argc++;

	cli = json_load_cli(argc, (const char **) p->argv);
	cr_assert_not_null(cli);

	//json_dumpf(json, stdout, JSON_INDENT(2)); putc('\n', stdout);
	//json_dumpf(cli, stdout, JSON_INDENT(2)); putc('\n', stdout);

	cr_assert(json_equal(json, cli));
}
