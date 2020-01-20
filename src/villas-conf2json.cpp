/** Convert old style config to new JSON format.
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

#include <libgen.h>
#include <iostream>
#include <jansson.h>
#include <libconfig.h>

#include <villas/config.h>
#include <villas/config_helper.hpp>
#include <villas/utils.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

class Config2Json : public Tool {

public:
	Config2Json(int argc, char *argv[]) :
		Tool(argc, argv, "conf2json")
	{ }

protected:

	void usage()
	{
		std::cout << "Usage: conf2json input.conf > output.json" << std::endl << std::endl;

		printCopyright();
	}

	int main()
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
		if(f == nullptr)
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
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[])
{
	villas::node::tools::Config2Json t(argc, argv);

	return t.run();
}
