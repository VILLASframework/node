/** Unit tests for libjansson helpers
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/utils.h>
#include <villas/config_helper.h>

using str = std::basic_string<char, std::char_traits<char>, criterion::allocator<char>>;

struct param {
	std::vector<str, criterion::allocator<str>> argv;
	str json;
};

ParameterizedTestParameters(json, json_load_cli)
{
	static struct param params[] = {
		// Combined long option
		{
			.argv = { "dummy", "--option=value" },
			.json = "{ \"option\" : \"value\" }"
		},
		// Separated long option
		{
			.argv = { "dummy", "--option", "value" },
			.json = "{ \"option\" : \"value\" }"
		},
		// All kinds of data types
		{
			.argv = { "dummy", "--integer", "1", "--real", "1.1", "--bool", "true", "--null", "null", "--string", "hello world" },
			.json = "{ \"integer\" : 1, \"real\" : 1.1, \"bool\" : true, \"null\" : null, \"string\" : \"hello world\" }"
		},
		// Array generation
		{
			.argv = { "dummy", "--bool", "true", "--bool", "false" },
			.json = "{ \"bool\" : [ true, false ] }"
		},
		// Dots in the option name generate sub objects
		{
			.argv = { "dummy", "--sub.option", "value" },
			.json = "{ \"sub\" : { \"option\" : \"value\" } }"
		},
		// Nesting is possible
		{
			.argv = { "dummy", "--sub.sub.option", "value" },
			.json = "{ \"sub\" : { \"sub\" : { \"option\" : \"value\" } } }"
		},
		// Multiple subgroups are merged
		{
			.argv = { "dummy", "--sub.sub.option", "value1", "--sub.option", "value2" },
			.json = "{ \"sub\" : { \"option\" : \"value2\", \"sub\" : { \"option\" : \"value1\" } } }"
		}
	};

	return criterion_test_params(params);
}

ParameterizedTest(struct param *p, json, json_load_cli)
{
	json_error_t err;
	json_t *json, *cli;

	json = json_loads(p->json.c_str(), 0, &err);
	cr_assert_not_null(json);

	auto argv = new const char*[p->argv.size() + 1];
	for (int i = 0; i < p->argv.size(); i++)
		argv[i] = p->argv[i].c_str();
	argv[p->argv.size()] = nullptr;

	cli = json_load_cli(p->argv.size(), argv);
	cr_assert_not_null(cli);

	//json_dumpf(json, stdout, JSON_INDENT(2)); putc('\n', stdout);
	//json_dumpf(cli, stdout, JSON_INDENT(2)); putc('\n', stdout);

	cr_assert(json_equal(json, cli));
}
