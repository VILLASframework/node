/** Configuration file parsing.
 *
 * @file
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

#pragma once

#include <functional>
#include <regex>
#include <jansson.h>

#include <villas/node/config.h>
#include <villas/advio.hpp>

namespace villas {
namespace node {

class Config {

protected:
	using str_walk_fcn_t = std::function<json_t *(json_t *)>;

	Logger logger;

	/** Check if file exists on local system. */
	static bool isLocalFile(const std::string &uri)
	{
		return access(uri.c_str(), F_OK) != -1;
	}

	/** Decode configuration file. */
	json_t * decode(FILE *f);

#ifdef WITH_CONFIG
	/** Convert libconfig .conf file to libjansson .json file. */
	json_t * libconfigDecode(FILE *f);
#endif /* WITH_CONFIG */

	/** Load configuration from standard input (stdim). */
	FILE * loadFromStdio();

	/** Load configuration from local file. */
	FILE * loadFromLocalFile(const std::string &u);

	/** Load configuration from a remote URI via advio. */
	AFILE * loadFromRemoteFile(const std::string &u);

	/** Resolve custom include directives. */
	void resolveIncludes();

	/** To shell-like subsitution of environment variables in strings. */
	void expandEnvVars();

	/** Run a callback function for each string in the config */
	json_t * walkStrings(json_t *root, str_walk_fcn_t cb);

public:
	json_t *root;

	Config();
	Config(const std::string &u);

	~Config();

	json_t * load(const std::string &u);
};

} /* namespace node */
} /* namespace villas */
