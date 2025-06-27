/* Logging.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <list>
#include <string>

#include <jansson.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace villas {

// Forward declarations
using Logger = std::shared_ptr<spdlog::logger>;
class Log {

public:
  using Level = spdlog::level::level_enum;
  using DefaultSink = std::shared_ptr<spdlog::sinks::stderr_color_sink_mt>;
  using DistSink = std::shared_ptr<spdlog::sinks::dist_sink_mt>;
  using Formatter = std::shared_ptr<spdlog::pattern_formatter>;

  class Expression {
  public:
    std::string name;
    Level level;

    Expression(const std::string &n, Level lvl) : name(n), level(lvl) {}

    Expression(json_t *json);
  };

protected:
  DistSink sinks;
  DefaultSink sink;
  Formatter formatter;

  Level level;

  std::string pattern; // Logging format.
  std::string prefix;  // Prefix each line with this string.

  std::list<Expression> expressions;

public:
  Log(Level level = Level::info);

  // Get the real usable log output width which fits into one line.
  static int getWidth();

  void parse(json_t *json);

  static Log &getInstance() {
    // This will "leak" memory. Because we don't have a complex destructor it's okay
    // that this will be cleaned up implicitly on program exit.
    static auto log = new Log();
    return *log;
  };

  Logger getNewLogger(const std::string &name);

  static Logger get(const std::string &name) {
    static auto &log = getInstance();
    return log.getNewLogger(name);
  }

  void setFormatter(const std::string &pattern, const std::string &pfx = "");
  void setLevel(Level lvl);
  void setLevel(const std::string &lvl);

  Level getLevel() const;
  std::string getLevelName() const;

  void addSink(std::shared_ptr<spdlog::sinks::sink> sink) {
    sink->set_formatter(formatter->clone());
    sink->set_level(level);

    sinks->add_sink(sink);
  }

  void replaceStdSink(std::shared_ptr<spdlog::sinks::sink> sink) {
    sink->set_formatter(formatter->clone());
    sink->set_level(level);

    sinks->sinks()[0] = sink;
  }
};

} // namespace villas
