/** Convert old style config to new JSON format.
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

#include <libgen.h>
#include <iostream>
#include <jansson.h>
#include <libconfig.h>

#include <villas/config_helper.h>
#include <villas/utils.h>

void usage()
{
	std::cout << "Usage: conf2json input.conf > output.json" << std::endl << std::endl;

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	config_t cfg;
	config_setting_t *cfg_root;
	json_t *json;

	if (argc != 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	FILE *f = fopen(argv[1], "r");
	if(f == NULL)
		return -1;

	const char *confdir = dirname(argv[1]);

	config_init(&cfg);

	config_set_include_dir(&cfg, confdir);

	ret = config_read(&cfg, f);
	if (ret != CONFIG_TRUE)
		return -2;

	cfg_root = config_root_setting(&cfg);

	json = config_to_json(cfg_root);
	if (!json)
		return -3;

	ret = json_dumpf(json, stdout, JSON_INDENT(2)); fflush(stdout);
	if (ret)
		return ret;

	json_decref(json);
	config_destroy(&cfg);

	return 0;
}
