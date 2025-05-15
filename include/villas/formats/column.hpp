/* Comma-separated values.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>

#include <villas/formats/line.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ColumnLineFormat : public LineFormat {

protected:
  size_t sprintLine(char *buf, size_t len, const struct Sample *smp) override;
  size_t sscanLine(const char *buf, size_t len, struct Sample *smp) override;

  char separator; // Column separator

public:
  ColumnLineFormat(int fl, char delim, char sep)
      : LineFormat(fl, delim), separator(sep) {}

  void header(FILE *f, const SignalList::Ptr sigs) override;

  void parse(json_t *json) override;
};

template <const char *name, const char *desc, int flags = 0,
          char delimiter = '\n', char separator = '\t'>
class ColumnLineFormatPlugin : public FormatFactory {

public:
  using FormatFactory::FormatFactory;

  Format *make() override {
    return new ColumnLineFormat(flags, delimiter, separator);
  }

  // Get plugin name
  std::string getName() const override { return name; }

  // Get plugin description
  std::string getDescription() const override { return desc; }
};

} // namespace node
} // namespace villas
