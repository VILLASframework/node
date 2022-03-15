/** Configuration file parsing.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <linux/limits.h>
#include <unistd.h>
#include <libgen.h>
#include <glob.h>

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <villas/node/config.hpp>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/config_class.hpp>
#include <villas/boxes.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/config_helper.hpp>

#ifdef WITH_CONFIG
  #include <libconfig.h>
#endif

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
	json_decref(root);
}

json_t * Config::load(std::FILE *f, bool resolveInc, bool resolveEnvVars)
{
	json_t *root = decode(f);

	if (resolveInc) {
		json_t *root_old = root;
		root = expandIncludes(root);
		json_decref(root_old);
	}

	if (resolveEnvVars) {
		json_t *root_old = root;
		root = expandEnvVars(root);
		json_decref(root_old);
	}

	return root;
}

json_t * Config::load(const std::string &u, bool resolveInc, bool resolveEnvVars)
{
	FILE *f;

	if (u == "-")
		f = loadFromStdio();
	else
		f = loadFromLocalFile(u);

	json_t *root = load(f, resolveInc, resolveEnvVars);

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

json_t * Config::decode(FILE *f)
{
	json_error_t err;

	// Update list of include directories
	auto incDirs = getIncludeDirectories(f);
	includeDirectories.insert(includeDirectories.end(), incDirs.begin(), incDirs.end());

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

std::list<std::string> Config::getIncludeDirectories(FILE *f) const
{
	int ret, fd;
	char buf[PATH_MAX];
	char *dir;

	std::list<std::string> dirs;

	// Adding directory of base configuration file
	fd = fileno(f);
	if (fd < 0)
		throw SystemError("Failed to get file descriptor");

	auto path = fmt::format("/proc/self/fd/{}", fd);

	ret = readlink(path.c_str(), buf, sizeof(buf));
	if (ret > 0) {
		buf[ret] = 0;
		if (isLocalFile(buf)) {
			dir = dirname(buf);
			dirs.push_back(dir);
		}
	}

	// Adding current working directory
	dir = getcwd(buf, sizeof(buf));
	if (dir != nullptr)
		dirs.push_back(dir);

	return dirs;
}

std::list<std::string> Config::resolveIncludes(const std::string &n)
{
	glob_t gb;
	int ret, flags = 0;

	memset(&gb, 0, sizeof(gb));

	auto name = n;
	resolveEnvVars(name);

	if (name.size() >= 1 && name[0] == '/') {  // absolute path
		ret = glob(name.c_str(), flags, nullptr, &gb);
		if (ret && ret != GLOB_NOMATCH)
			gb.gl_pathc = 0;
	}
	else { // relative path
		for (auto &dir : includeDirectories) {
			auto pattern = fmt::format("{}/{}", dir, name.c_str());

			ret = glob(pattern.c_str(), flags, nullptr, &gb);
			if (ret && ret != GLOB_NOMATCH) {
				gb.gl_pathc = 0;

				goto out;
			}

			flags |= GLOB_APPEND;
		}
	}

out:	std::list<std::string> files;
	for (unsigned i = 0; i < gb.gl_pathc; i++)
		files.push_back(gb.gl_pathv[i]);

	globfree(&gb);

	return files;
}

void Config::resolveEnvVars(std::string &text)
{
	static const std::regex env_re{R"--(\$\{([^}]+)\})--"};

	std::smatch match;
	while (std::regex_search(text, match, env_re)) {
		auto const from = match[0];
		auto const var_name = match[1].str().c_str();
		char *var_value = std::getenv(var_name);
		if (!var_value)
			throw RuntimeError("Unresolved environment variable: {}", var_name);

		text.replace(from.first - text.begin(), from.second - from.first, var_value);

		logger->debug("Replace env var {} in \"{}\" with value \"{}\"",
			var_name, text, var_value);
	}
}

#ifdef WITH_CONFIG
#if (LIBCONFIG_VER_MAJOR > 1) || ((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 7))
const char ** Config::includeFuncStub(config_t *cfg, const char *include_dir, const char *path, const char **error)
{
	void *ctx = config_get_hook(cfg);

	return reinterpret_cast<Config *>(ctx)->includeFunc(cfg, include_dir, path, error);
}

const char ** Config::includeFunc(config_t *cfg, const char *include_dir, const char *path, const char **error)
{
	auto paths = resolveIncludes(path);

	unsigned i = 0;
	auto files = (const char **) malloc(sizeof(char **) * (paths.size() + 1));

	for (auto &path : paths)
		files[i++] = strdup(path.c_str());

	files[i] = NULL;

  	return files;
}
#endif

json_t * Config::libconfigDecode(FILE *f)
{
	int ret;

	config_t cfg;
	config_setting_t *cfg_root;
	config_init(&cfg);
	config_set_auto_convert(&cfg, 1);

	/* Setup libconfig include path. */
#if (LIBCONFIG_VER_MAJOR > 1) || ((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 7))
	config_set_hook(&cfg, this);

	config_set_include_func(&cfg, includeFuncStub);
#else
	if (includeDirectories.size() > 0) {
		logger->info("Setting include dir to: {}", includeDirectories.front());

		config_set_include_dir(&cfg, includeDirectories.front().c_str());

		if (includeDirectories.size() > 1) {
			logger->warn("Ignoring all but the first include directories for libconfig");
			logger->warn("  libconfig does not support more than a single include dir!");
		}
	}
#endif

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
			return json_incref(root);
	};
}

json_t * Config::expandEnvVars(json_t *in)
{
	return walkStrings(in, [this](json_t *str) -> json_t * {
		std::string text = json_string_value(str);

		resolveEnvVars(text);

		return json_string(text.c_str());
	});
}

json_t * Config::expandIncludes(json_t *in)
{
	return walkStrings(in, [this](json_t *str) -> json_t * {
		int ret;
		std::string text = json_string_value(str);
		static const std::string kw = "@include ";

		if (text.find(kw) != 0)
			return json_incref(str);
		else {
			std::string pattern = text.substr(kw.size());

			resolveEnvVars(pattern);

			json_t *incl = nullptr;

			for (auto &path : resolveIncludes(pattern)) {
				json_t *other = load(path);
				if (!other)
					throw ConfigError(str, "Failed to include config file from {}", path);

				if (!incl)
					incl = other;
				else if (json_is_object(incl) && json_is_object(other)) {
					ret = json_object_update(incl, other);
					if (ret)
						throw ConfigError(str, "Can not mix object and array-typed include files");
				}
				else if (json_is_array(incl) && json_is_array(other)) {
					ret = json_array_extend(incl, other);
					if (ret)
						throw ConfigError(str, "Can not mix object and array-typed include files");
				}

				logger->debug("Included config from: {}", path);
			}

			return incl;
		}
	});
}
