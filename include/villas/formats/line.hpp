/* Line-based formats.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>

namespace villas {
namespace node {

class LineFormat : public Format {

protected:
  virtual size_t sprintLine(char *buf, size_t len,
                            const struct Sample *smp) = 0;
  virtual size_t sscanLine(const char *buf, size_t len, struct Sample *smp) = 0;

  char delimiter; // Newline delimiter.
  char comment;   // Prefix for comment lines.

  bool skip_first_line; // While reading, the first line is skipped (header)
  bool print_header;    // Before any data, a header line is printed

  bool first_line_skipped;
  bool header_printed;

public:
  LineFormat(int fl, char delim = '\n', char com = '#')
      : Format(fl), delimiter(delim), comment(com), skip_first_line(false),
        print_header(true), first_line_skipped(false), header_printed(false) {}

  // Print a header
  virtual void header(FILE *f, const SignalList::Ptr sigs) {
    header_printed = true;
  }

  virtual int sprint(char *buf, size_t len, size_t *wbytes,
                     const struct Sample *const smps[], unsigned cnt);
  virtual int sscan(const char *buf, size_t len, size_t *rbytes,
                    struct Sample *const smps[], unsigned cnt);

  virtual int scan(FILE *f, struct Sample *const smps[], unsigned cnt);
  virtual int print(FILE *f, const struct Sample *const smps[], unsigned cnt);

  virtual void parse(json_t *json);
};

template <typename T, const char *name, const char *desc, int flags = 0,
          char delimiter = '\n'>
class LineFormatPlugin : public FormatFactory {

public:
  using FormatFactory::FormatFactory;

  virtual Format *make() { return new T(flags, delimiter); }

  /// Get plugin name
  virtual std::string getName() const { return name; }

  /// Get plugin description
  virtual std::string getDescription() const { return desc; }
};

} // namespace node
} // namespace villas
