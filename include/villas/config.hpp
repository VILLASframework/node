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

#include <jansson.h>

#include <villas/node/config.h>
#include <villas/advio.hpp>

namespace villas {
namespace node {

class Config {

protected:
	std::string uri;

	FILE *local_file;
	AFILE *remote_file;

	Logger logger;

	/** Check if file exists on local system. */
	static bool isLocalFile(const std::string &uri)
	{
		return access(uri.c_str(), F_OK) != -1;
	}

	/** Decode configuration file. */
	void decode();

#ifdef WITH_CONFIG
	/** Convert libconfig .conf file to libjansson .json file. */
	void libconfigDecode();
#endif /* WITH_CONFIG */

	/** Load configuration from standard input (stdim). */
	void loadFromStdio();

	/** Load configuration from local file. */
	void loadFromLocalFile(const std::string &u);

	/** Load configuration from a remote URI via advio. */
	void loadFromRemoteFile(const std::string &u);

public:
	json_t *root;

	Config();
	Config(const std::string &u);

	~Config();

	void load(const std::string &u);

	/** Pretty-print libjansson error. */
	void prettyPrintError(json_error_t err);
};

} // namespace node
} // namespace villas
