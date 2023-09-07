/* Line-based formats.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/formats/line.hpp>

using namespace villas;
using namespace villas::node;

int LineFormat::sprint(char *buf, size_t len, size_t *wbytes,
                       const struct Sample *const smps[], unsigned cnt) {
  unsigned i;
  size_t off = 0;

  for (i = 0; i < cnt && off < len; i++)
    off += sprintLine(buf + off, len - off, smps[i]);

  if (wbytes)
    *wbytes = off;

  return i;
}

int LineFormat::sscan(const char *buf, size_t len, size_t *rbytes,
                      struct Sample *const smps[], unsigned cnt) {
  unsigned i;
  size_t off = 0;

  if (skip_first_line) {
    if (!first_line_skipped) {
      while (off < len) {
        if (buf[off++] == delimiter)
          break;
      }

      first_line_skipped = true;
    }
  }

  for (i = 0; i < cnt && off < len; i++) {
    // Skip comment lines
    if (buf[off] == comment) {
      while (off < len) {
        if (buf[++off] == delimiter)
          break;
      }

      if (off == len)
        break;
    }

    off += sscanLine(buf + off, len - off, smps[i]);
  }

  if (rbytes)
    *rbytes = off;

  return i;
}

int LineFormat::print(FILE *f, const struct Sample *const smps[],
                      unsigned cnt) {
  int ret;
  unsigned i;

  if (cnt > 0 && smps[0]->signals)
    header(f, smps[0]->signals);

  for (i = 0; i < cnt; i++) {
    size_t wbytes;

    ret = sprint(out.buffer, out.buflen, &wbytes, &smps[i], 1);
    if (ret < 0)
      return ret;

    fwrite(out.buffer, wbytes, 1, f);
  }

  return i;
}

int LineFormat::scan(FILE *f, struct Sample *const smps[], unsigned cnt) {
  int ret;
  unsigned i;
  ssize_t bytes;

  if (skip_first_line) {
    if (!first_line_skipped) {
      bytes = getdelim(&in.buffer, &in.buflen, delimiter, f);
      if (bytes < 0)
        return -1; // An error or eof occured

      first_line_skipped = true;
    }
  }

  for (i = 0; i < cnt && !feof(f); i++) {
    size_t rbytes;
    char *ptr;

  skip:
    bytes = getdelim(&in.buffer, &in.buflen, delimiter, f);
    if (feof(f))
      break;
    else if (bytes < 0)
      return -1; // An error or eof occured

    // Skip whitespaces, empty and comment lines
    for (ptr = in.buffer; isspace(*ptr); ptr++)
      ;

    if (ptr[0] == '\0' || ptr[0] == comment)
      goto skip;

    ret = sscan(in.buffer, bytes, &rbytes, &smps[i], 1);
    if (ret < 0)
      return ret;
  }

  return i;
}

void LineFormat::parse(json_t *json) {
  int ret;
  json_error_t err;
  const char *delim = nullptr;
  const char *com = nullptr;
  int header = -1;
  int skip = -1;

  ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: b, s?: b, s?: s }",
                       "delimiter", &delim, "header", &header,
                       "skip_first_line", &skip, "comment_prefix", &com);
  if (ret)
    throw ConfigError(json, err, "node-config-format-line",
                      "Failed to parse format configuration");

  if (delim) {
    if (strlen(delim) != 1)
      throw ConfigError(json, "node-config-format-line-delimiter",
                        "Line delimiter must be a single character!");

    delimiter = delim[0];
  }

  if (com) {
    if (strlen(com) != 1)
      throw ConfigError(json, "node-config-format-line-comment_prefix",
                        "Comment prefix must be a single character!");

    comment = com[0];
  }

  if (header >= 0)
    print_header = header;

  if (skip >= 0)
    skip_first_line = skip;

  Format::parse(json);
}
