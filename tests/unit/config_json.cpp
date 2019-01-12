/** Unit tests libconfig to jansson converters.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#ifdef LIBCONFIG_FOUND

#include <jansson.h>
#include <libconfig.h>

#include <villas/utils.h>
#include <villas/config_helper.h>

const char *cfg_example = "test : \n"
			  "{\n"
			  "  hallo = 1L;\n"
			  "};\n"
			  "liste = ( 1.1, 2L, 3L, 4L, \n"
			  "  {\n"
			  "    objekt : \n"
			  "    {\n"
			  "      key = \"value\";\n"
			  "    };\n"
			  "  } );\n";

const char *json_example = "{\n"
			   "  \"test\": {\n"
			   "    \"hallo\": 1\n"
			   "  },\n"
			   "  \"liste\": [\n"
			   "    1.1000000000000001,\n"
			   "    2,\n"
			   "    3,\n"
			   "    4,\n"
			   "    {\n"
			   "      \"objekt\": {\n"
			   "        \"key\": \"value\"\n"
			   "      }\n"
			   "    }\n"
			   "  ]\n"
			   "}";

Test(utils, config_to_json)
{
	int ret;
	config_t cfg;
	config_setting_t *cfg_root;
	json_t *json;

	config_init(&cfg);

	ret = config_read_string(&cfg, cfg_example);
	cr_assert_eq(ret, CONFIG_TRUE);

	cfg_root = config_root_setting(&cfg);

	json = config_to_json(cfg_root);
	cr_assert_not_null(json);

	char *str = json_dumps(json, JSON_INDENT(2));

	//printf("%s\n", str);

	json_decref(json);

	cr_assert_str_eq(str, json_example);

	config_destroy(&cfg);
}

Test(utils, json_to_config)
{
	config_t cfg;
	config_setting_t *cfg_root;
	json_t *json;

	/* For config_write() */
	FILE *f;
	char str[1024];

	config_init(&cfg);

	cfg_root = config_root_setting(&cfg);

	json = json_loads(json_example, 0, nullptr);
	cr_assert_not_null(json);

	json_to_config(json, cfg_root);

	//config_write(&cfg, stdout);

	f = fmemopen(str, sizeof(str), "w+");
	config_write(&cfg, f);
	fclose(f);

	cr_assert_str_eq(str, cfg_example);

	json_decref(json);
}

#endif /* LIBCONFIG_FOUND */
