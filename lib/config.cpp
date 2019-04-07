
/** Configuration file parsing.
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

#include <unistd.h>
#include <libgen.h>

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <villas/utils.h>
#include <villas/log.hpp>
#include <villas/config.hpp>
#include <villas/boxes.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/config_helper.hpp>

using namespace villas;
using namespace villas::node;

Config::Config() :
	local_file(nullptr),
	remote_file(nullptr),
	root(nullptr)
{
	logger = logging.get("config");
}

Config::Config(const std::string &u) :
	Config()
{
	load(u);
}

Config::~Config()
{
	/* Close configuration file */
	if (remote_file)
		afclose(remote_file);
	else if (local_file && local_file != stdin)
		fclose(local_file);

	if (root)
		json_decref(root);
}

void Config::load(const std::string &u)
{
	if (u == "-")
		loadFromStdio();
	else if (isLocalFile(u))
		loadFromLocalFile(u);
	else
		loadFromRemoteFile(u);

	decode();
}

void Config::loadFromStdio()
{
	logger->info("Reading configuration from standard input");

	local_file = stdin;
}

void Config::loadFromLocalFile(const std::string &u)
{
	logger->info("Reading configuration from local file: {}", u);

	local_file = fopen(u.c_str(), "r");
	if (!local_file)
		throw RuntimeError("Failed to open configuration from: {}", u);

	uri = u;
}

void Config::loadFromRemoteFile(const std::string &u)
{
	logger->info("Reading configuration from remote URI: {}", u);

	remote_file = afopen(u.c_str(), "r");
	if (!remote_file)
		throw RuntimeError("Failed to open configuration from: {}", u);

	local_file = remote_file->file;

	uri = u;
}

void Config::decode()
{
	json_error_t err;

	root = json_loadf(local_file, 0, &err);
	if (root == nullptr) {
#ifdef WITH_CONFIG
		/* We try again to parse the config in the legacy format */
		libconfigDecode();
#else
		throw JanssonParseError(err);
#endif /* WITH_CONFIG */
	}
}

#ifdef WITH_CONFIG
void Config::libconfigDecode()
{
	int ret;

	config_t cfg;
	config_setting_t *cfg_root;
	config_init(&cfg);
	config_set_auto_convert(&cfg, 1);

	/* Setup libconfig include path.
	* This is only supported for local files */
	if (isLocalFile(uri)) {
		char *cpy = strdup(uri.c_str());

		config_set_include_dir(&cfg, dirname(cpy));

		free(cpy);
	}

	if (remote_file)
		arewind(remote_file);
	else
		rewind(local_file);

	ret = config_read(&cfg, local_file);
	if (ret != CONFIG_TRUE)
		throw LibconfigParseError(&cfg);

	cfg_root = config_root_setting(&cfg);

	root = config_to_json(cfg_root);
	if (root == nullptr)
		throw RuntimeError("Failed to convert JSON to configuration file");

	config_destroy(&cfg);
}
#endif /* WITH_CONFIG */

void Config::prettyPrintError(json_error_t err)
{
	std::ifstream infile(uri);
	std::string line;

	int context = 4;
	int start_line = err.line - context;
	int end_line = err.line + context;

	for (int line_no = 0; std::getline(infile, line); line_no++) {
		if (line_no < start_line || line_no > end_line)
			continue;

		std::cerr << std::setw(4) << std::right << line_no << " " BOX_UD " " << line << std::endl;

		if (line_no == err.line) {
			for (int col = 0; col < err.column; col++)
				std::cerr << " ";

			std::cerr << BOX_UD;

			for (int col = 0; col < err.column; col++)
				std::cerr << " ";

			std::cerr << BOX_UR << err.text << std::endl;
			std::cerr << std::endl;
		}
	}
}
