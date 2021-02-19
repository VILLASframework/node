
/** Configuration file parsing.
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

#include <unistd.h>
#include <libgen.h>

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>

#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/config.hpp>
#include <villas/boxes.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/config_helper.hpp>

namespace fs = std::filesystem;

using namespace villas;
using namespace villas::node;

Config::Config() :
	logger(logging.get("config")),
	root(nullptr)
{ }

Config::Config(const std::string &u) :
	Config()
{
	root = load(u);
}

Config::~Config()
{
	if (root)
		json_decref(root);
}

json_t * Config::load(std::FILE *f, bool resolveInc, bool resolveEnvVars)
{
	json_t *root = decode(f);

	if (resolveInc)
		root = resolveIncludes(root);

	if (resolveEnvVars)
		root = expandEnvVars(root);

	return root;
}

json_t * Config::load(const std::string &u, bool resolveInc, bool resolveEnvVars)
{
	FILE *f;
	AFILE *af = nullptr;

	if (u == "-")
		f = loadFromStdio();
	else if (isLocalFile(u))
		f = loadFromLocalFile(u);
	else {
		af = loadFromRemoteFile(u);
		f = af->file;
	}

	json_t *root = load(f, resolveInc, resolveEnvVars);

	if (af)
		afclose(af);
	else
		fclose(f);

	return root;
}

FILE * Config::loadFromStdio()
{
	logger->info("Reading configuration from standard input");

	return stdin;
}

FILE * Config::loadFromLocalFile(const std::string &u)
{
	logger->info("Reading configuration from local file: {}", u);

	FILE *f = fopen(u.c_str(), "r");
	if (!f)
		throw RuntimeError("Failed to open configuration from: {}", u);

	return f;
}

AFILE * Config::loadFromRemoteFile(const std::string &u)
{
	logger->info("Reading configuration from remote URI: {}", u);

	AFILE *f = afopen(u.c_str(), "r");
	if (!f)
		throw RuntimeError("Failed to open configuration from: {}", u);

	return f;
}

json_t * Config::decode(FILE *f)
{
	json_error_t err;

	json_t *root = json_loadf(f, 0, &err);
	if (root == nullptr) {
#ifdef WITH_CONFIG
		/* We try again to parse the config in the legacy format */
		root = libconfigDecode(f);
#else
		throw JanssonParseError(err);
#endif /* WITH_CONFIG */
	}

	return root;
}

std::list<std::filesystem::path> Config::getIncludeDirs(FILE *f) const
{
	auto uri = fs::read_symlink(fs::path("/proc/self/fd") / std::to_string(fileno(f)));
	if (isLocalFile(uri)) {
		return {
			uri.parent_path()
		};
	}
	else
		return { };
}

#ifdef WITH_CONFIG
json_t * Config::libconfigDecode(FILE *f)
{
	int ret;

	config_t cfg;
	config_setting_t *cfg_root;
	config_init(&cfg);
	config_set_auto_convert(&cfg, 1);

	/* Setup libconfig include path. */
	auto inclDirs = getIncludeDirs(f);
	if (inclDirs.size() > 0) {
		logger->info("Setting include dir to: {}", inclDirs.front());

		config_set_include_dir(&cfg, inclDirs.front().c_str());

		if (inclDirs.size() > 1) {
			logger->warn("Ignoring all but the first include directories for libconfig");
			logger->warn("  libconfig does not support more than a single include dir!");
		}
	}

	/* Rewind before re-reading */
	rewind(f);

	ret = config_read(&cfg, f);
	if (ret != CONFIG_TRUE)
		throw LibconfigParseError(&cfg);

	cfg_root = config_root_setting(&cfg);

	json_t *root = config_to_json(cfg_root);
	if (!root)
		throw RuntimeError("Failed to convert JSON to configuration file");

	config_destroy(&cfg);

	return root;
}
#endif /* WITH_CONFIG */

json_t * Config::walkStrings(json_t *root, str_walk_fcn_t cb)
{
	const char *key;
	size_t index;
	json_t *val, *new_val, *new_root;

	switch (json_typeof(root)) {
		case JSON_STRING:
			return cb(root);

		case JSON_OBJECT:
			new_root = json_object();

			json_object_foreach(root, key, val) {
				new_val = walkStrings(val, cb);

				json_object_set_new(new_root, key, new_val);
			}

			return new_root;

		case JSON_ARRAY:
			new_root = json_array();

			json_array_foreach(root, index, val) {
				new_val = walkStrings(val, cb);

				json_array_append_new(new_root, new_val);
			}

			return new_root;

		default:
			return root;
	};
}

json_t * Config::expandEnvVars(json_t *in)
{
	static const std::regex env_re{R"--(\$\{([^}]+)\})--"};

	return walkStrings(in, [this](json_t *str) -> json_t * {
		std::string text = json_string_value(str);

		std::smatch match;
		while (std::regex_search(text, match, env_re)) {
			auto const from = match[0];
			auto const var_name = match[1].str().c_str();
			char *var_value = std::getenv(var_name);

			text.replace(from.first - text.begin(), from.second - from.first, var_value);

			logger->debug("Replace env var {} in \"{}\" with value \"{}\"",
				var_name, text, var_value);
		}

		return json_string(text.c_str());
	});
}

json_t * Config::resolveIncludes(json_t *in)
{
	return walkStrings(in, [this](json_t *str) -> json_t * {
		std::string text = json_string_value(str);
		static const std::string kw = "@include ";

		if (text.find(kw) != 0)
			return str;
		else {
			std::string path = text.substr(kw.size());

			json_t *incl = load(path);
			if (!incl)
				throw ConfigError(str, "Failed to include config file from {}", path);

			logger->debug("Included config from: {}", path);

			return incl;
		}
	});
}
