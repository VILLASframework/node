/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <list>
#include <algorithm>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <villas/log.hpp>
#include <villas/exceptions.hpp>

using namespace villas;

/** The global log instance */
Log villas::logging;

Log::Log(Level level) :
	sinks(std::make_shared<DistSink::element_type>()),
	pattern("%H:%M:%S %P %^%l%$: %v")
{
	char *p = getenv("VILLAS_LOG_PREFIX");
	if (p)
		prefix = p;

	spdlog::set_level(level);
	spdlog::set_pattern(pattern, spdlog::pattern_time_type::utc);

	auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

	sink->set_pattern(prefix + pattern);
	sinks->add_sink(sink);
}

int Log::getWidth()
{
	int width = Terminal::getCols() - 25;

	if (!prefix.empty())
		width -= prefix.length();

	return width;
}

Logger Log::get(const std::string &name)
{
	auto logger = spdlog::get(name);

	if (not logger)
		logger = std::make_shared<Logger::element_type>(name, sinks);

	return logger;
}

void Log::parse(json_t *cfg)
{
	const char *level = NULL;
	const char *path = NULL;
	const char *pattern = NULL;

	int syslog;
	int ret;

	json_error_t err;
	json_t *json_expressions;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: b, s?: s }",
		"level", &level,
		"file", &path,
		"expressions", &json_expressions,
		"syslog", &syslog,
		"pattern", &pattern
	);
	if (ret)
		throw new JsonError(err);

	if (level)
		setLevel(level);

	if (path) {
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);

		sinks->add_sink(sink);
	}

	if (syslog) {
		auto sink = std::make_shared<spdlog::sinks::syslog_sink_mt>("villas", LOG_PID, LOG_DAEMON);

		sinks->add_sink(sink);
	}

	if (json_expressions) {
		if (!json_is_array(json_expressions))
			throw new ConfigError(json_expressions, "node-config.html#node-config-logging-expressions", "The 'expressions' setting must be a list of objects.");

		size_t i;
		json_t *json_expression;
		json_array_foreach(json_expressions, i, json_expression) {
			const char *name;
			const char *lvl;

			ret = json_unpack_ex(json_expression, &err, 0, "{ s: s, s: s }",
				"name", &name,
				"level", &lvl
			);
			if (ret)
				throw new JsonError(err);

			auto logger = get(name);
			auto level = spdlog::level::from_str(lvl);

			logger->set_level(level);
		}
	}
}

void Log::setPattern(const std::string &pat)
{
	pattern = pat;

	spdlog::set_pattern(pattern, spdlog::pattern_time_type::utc);
}

void Log::setLevel(Level lvl)
{
	spdlog::set_level(lvl);
}

void Log::setLevel(const std::string &lvl)
{
	std::list<std::string> l = SPDLOG_LEVEL_NAMES;

	auto it = std::find(l.begin(), l.end(), lvl);
	if (it == l.end())
		throw RuntimeError("Invalid log level {}", lvl);

	level = spdlog::level::from_str(lvl);

	setLevel(level);
}

Log::Level Log::getLevel() const
{
	return level;
}
