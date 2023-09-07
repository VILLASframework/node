/* VILLASnode exceptions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/exceptions.hpp>
#include <villas/node/config.hpp>

#ifdef WITH_CONFIG
#include <libconfig.h>
#endif

namespace villas {
namespace node {

class ParseError : public RuntimeError {

protected:
  std::string text;
  std::string file;
  int line;
  int column;

public:
  ParseError(const std::string &t, const std::string &f, int l, int c = 0)
      : RuntimeError("Failed to parse configuration: {} in {}:{}", t, f, l),
        text(t), file(f), line(l), column(c) {}
};

#ifdef WITH_CONFIG
class LibconfigParseError : public ParseError {

protected:
  const config_t *config;

public:
  LibconfigParseError(const config_t *c)
      : ParseError(config_error_text(c),
                   config_error_file(c) ? config_error_file(c) : "<unknown>",
                   config_error_line(c)),
        config(c) {}
};
#endif // WITH_CONFIG

class JanssonParseError : public ParseError {

protected:
  json_error_t error;

public:
  JanssonParseError(json_error_t e)
      : ParseError(e.text, e.source, e.line, e.column), error(e) {}
};

} // namespace node
} // namespace villas
