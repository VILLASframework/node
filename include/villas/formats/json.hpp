/* JSON serializtion sample data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class JsonFormat : public Format {

protected:
  static enum SignalType detect(const json_t *val);

  json_t *packFlags(const struct Sample *smp);
  int unpackFlags(json_t *json_ts, struct Sample *smp);

  json_t *packTimestamps(const struct Sample *smp);
  int unpackTimestamps(json_t *json_ts, struct Sample *smp);

  virtual int packSample(json_t **j, const struct Sample *smp);
  virtual int packSamples(json_t **j, const struct Sample *const smps[],
                          unsigned cnt);
  virtual int unpackSample(json_t *json_smp, struct Sample *smp);
  virtual int unpackSamples(json_t *json_smps, struct Sample *const smps[],
                            unsigned cnt);

  int dump_flags;

public:
  JsonFormat(int fl) : Format(fl), dump_flags(0) {}

  virtual int sscan(const char *buf, size_t len, size_t *rbytes,
                    struct Sample *const smps[], unsigned cnt);
  virtual int sprint(char *buf, size_t len, size_t *wbytes,
                     const struct Sample *const smps[], unsigned cnt);

  virtual int print(FILE *f, const struct Sample *const smps[], unsigned cnt);
  virtual int scan(FILE *f, struct Sample *const smps[], unsigned cnt);

  virtual void parse(json_t *json);
};

} // namespace node
} // namespace villas
