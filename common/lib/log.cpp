/* Logging and debugging routines.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <list>
#include <unordered_map>

#include <fnmatch.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/terminal.hpp>

using namespace villas;

static std::unordered_map<spdlog::level::level_enum, std::string> levelNames = {
    {spdlog::level::trace, "trc"}, {spdlog::level::debug, "dbg"},
    {spdlog::level::info, "info"}, {spdlog::level::warn, "warn"},
    {spdlog::level::err, "err"},   {spdlog::level::critical, "crit"},
    {spdlog::level::off, "off"}};

class CustomLevelFlag : public spdlog::custom_flag_formatter {

public:
  void format(const spdlog::details::log_msg &msg, const std::tm &,
              spdlog::memory_buf_t &dest) override {
    auto lvl = levelNames[msg.level];
    auto lvlpad = std::string(padinfo_.width_ - lvl.size(), ' ') + lvl;
    dest.append(lvlpad.data(), lvlpad.data() + lvlpad.size());
  }

  spdlog::details::padding_info get_padding_info() { return padinfo_; }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<CustomLevelFlag>();
  }
};

Log::Log(Level lvl) : level(lvl), pattern("%H:%M:%S %^%-4t%$ %-16n %v") {
  char *p = getenv("VILLAS_LOG_PREFIX");

  sinks = std::make_shared<DistSink::element_type>();

  setLevel(level);
  setFormatter(pattern, p ? p : "");

  // Default sink
  sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

  sinks->add_sink(sink);
}

int Log::getWidth() {
  int width = Terminal::getCols();

  width -= 8;  // Timestamp
  width -= 1;  // Space
  width -= 4;  // Level
  width -= 1;  // Space
  width -= 16; // Name
  width -= 1;  // Space
  width -= prefix.length();

  return width;
}

Logger Log::getNewLogger(const std::string &name) {
  Logger logger = spdlog::get(name);

  if (not logger) {
    logger = std::make_shared<Logger::element_type>(name, sinks);

    logger->set_level(level);
    logger->set_formatter(formatter->clone());

    for (auto &expr : expressions) {
      int flags = 0;
#ifdef FNM_EXTMATCH
      // musl-libc doesnt support this flag yet
      flags |= FNM_EXTMATCH;
#endif
      if (!fnmatch(expr.name.c_str(), name.c_str(), flags))
        logger->set_level(expr.level);
    }

    spdlog::register_logger(logger);
  }

  return logger;
}

void Log::parse(json_t *json) {
  const char *level = nullptr;
  const char *path = nullptr;
  const char *pattern = nullptr;

  int ret, syslog = 0;

  json_error_t err;
  json_t *json_expressions = nullptr;

  ret = json_unpack_ex(json, &err, JSON_STRICT,
                       "{ s?: s, s?: s, s?: o, s?: b, s?: s }", "level", &level,
                       "file", &path, "expressions", &json_expressions,
                       "syslog", &syslog, "pattern", &pattern);
  if (ret)
    throw ConfigError(json, err, "node-config-logging");

  if (level)
    setLevel(level);

  if (path) {
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);

    sinks->add_sink(sink);
  }

  if (syslog) {
#if SPDLOG_VERSION >= 10400
    auto sink = std::make_shared<spdlog::sinks::syslog_sink_mt>(
        "villas", LOG_PID, LOG_DAEMON, true);
#else
    auto sink = std::make_shared<spdlog::sinks::syslog_sink_mt>(
        "villas", LOG_PID, LOG_DAEMON);
#endif
    sinks->add_sink(sink);
  }

  if (json_expressions) {
    if (!json_is_array(json_expressions))
      throw ConfigError(json_expressions, "node-config-logging-expressions",
                        "The 'expressions' setting must be a list of objects.");

    size_t i;
    json_t *json_expression;

    // cppcheck-suppress unknownMacro
    json_array_foreach (json_expressions, i, json_expression)
      expressions.emplace_back(json_expression);
  }
}

void Log::setFormatter(const std::string &pat, const std::string &pfx) {
  pattern = pat;
  prefix = pfx;

  formatter = std::make_shared<spdlog::pattern_formatter>(
      spdlog::pattern_time_type::utc);
  formatter->add_flag<CustomLevelFlag>('t');
  formatter->set_pattern(prefix + pattern);

  sinks->set_formatter(formatter->clone());
}

void Log::setLevel(Level lvl) {
  level = lvl;

  sinks->set_level(lvl);
}

void Log::setLevel(const std::string &lvl) {
  auto l = SPDLOG_LEVEL_NAMES;

  auto it = std::find(l.begin(), l.end(), lvl);
  if (it == l.end())
    throw RuntimeError("Invalid log level {}", lvl);

  setLevel(spdlog::level::from_str(lvl));
}

Log::Level Log::getLevel() const { return level; }

std::string Log::getLevelName() const {
  auto sv = spdlog::level::to_string_view(level);

  return std::string(sv.data());
}

Log::Expression::Expression(json_t *json) {
  int ret;

  const char *nme;
  const char *lvl;

  json_error_t err;

  ret = json_unpack_ex(json, &err, JSON_STRICT, "{ s: s, s: s }", "name", &nme,
                       "level", &lvl);
  if (ret)
    throw ConfigError(json, err, "node-config-logging-expressions");

  level = spdlog::level::from_str(lvl);
  name = nme;
}
