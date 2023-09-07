/* Reading and writing simulation samples in various formats.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/node/config.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

Format *FormatFactory::make(json_t *json) {
  std::string type;
  Format *f;

  if (json_is_string(json)) {
    type = json_string_value(json);

    return FormatFactory::make(type);
  } else if (json_is_object(json)) {
    json_t *json_type = json_object_get(json, "type");

    type = json_string_value(json_type);

    f = FormatFactory::make(type);
    if (!f)
      return nullptr;

    f->parse(json);

    return f;
  } else
    throw ConfigError(json, "node-config-format", "Invalid format config");
}

Format *FormatFactory::make(const std::string &format) {
  FormatFactory *ff = plugin::registry->lookup<FormatFactory>(format);
  if (!ff)
    throw RuntimeError("Unknown format: {}", format);

  return ff->make();
}

Format::Format(int fl) : flags(fl), real_precision(17), signals(nullptr) {
  in.buflen = out.buflen = DEFAULT_FORMAT_BUFFER_LENGTH;

  in.buffer = new char[in.buflen];
  out.buffer = new char[out.buflen];

  if (!in.buffer || !out.buffer)
    throw MemoryAllocationError();
}

Format::~Format() {
  int ret __attribute__((unused));

  delete[] in.buffer;
  delete[] out.buffer;
}

void Format::start(SignalList::Ptr sigs, int fl) {
  flags &= fl;

  signals = sigs;

  start();
}

void Format::start(const std::string &dtypes, int fl) {
  flags |= fl;

  signals = std::make_shared<SignalList>(dtypes.c_str());
  if (!signals)
    throw MemoryAllocationError();

  start();
}

int Format::print(FILE *f, const struct Sample *const smps[], unsigned cnt) {
  int ret;
  size_t wbytes;

  ret = sprint(out.buffer, out.buflen, &wbytes, smps, cnt);

  fwrite(out.buffer, wbytes, 1, f);

  return ret;
}

int Format::scan(FILE *f, struct Sample *const smps[], unsigned cnt) {
  size_t bytes, rbytes;

  bytes = fread(in.buffer, 1, in.buflen, f);

  return sscan(in.buffer, bytes, &rbytes, smps, cnt);
}

void Format::parse(json_t *json) {
  int ret;
  json_error_t err;

  int ts_origin = -1;
  int ts_received = -1;
  int sequence = -1;
  int data = -1;
  int offset = -1;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s?: b, s?: b, s?: b, s?: b, s?: b, s?: i }",
                       "ts_origin", &ts_origin, "ts_received", &ts_received,
                       "sequence", &sequence, "data", &data, "offset", &offset,
                       "real_precision", &real_precision);
  if (ret)
    throw ConfigError(json, err, "node-config-format",
                      "Failed to parse format configuration");

  if (real_precision < 0 || real_precision > 31)
    throw ConfigError(json, err, "node-config-format-precision",
                      "The valid range for the real_precision setting is "
                      "between 0 and 31 (inclusive)");

  if (ts_origin == 0)
    flags &= ~(int)SampleFlags::HAS_TS_ORIGIN;

  if (ts_received == 0)
    flags &= ~(int)SampleFlags::HAS_TS_RECEIVED;

  if (sequence == 0)
    flags &= ~(int)SampleFlags::HAS_SEQUENCE;

  if (data == 0)
    flags &= ~(int)SampleFlags::HAS_DATA;

  if (offset == 0)
    flags &= ~(int)SampleFlags::HAS_OFFSET;
}
