/* Message related functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <cstring>

#include <villas/exceptions.hpp>
#include <villas/formats/msg.hpp>
#include <villas/formats/msg_format.hpp>
#include <villas/formats/villas_binary.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;

int VillasBinaryFormat::sprint(char *buf, size_t len, size_t *wbytes,
                               const struct Sample *const smps[],
                               unsigned cnt) {
  int ret;
  unsigned i = 0;
  char *ptr = buf;

  for (i = 0; i < cnt; i++) {
    struct Message *msg = (struct Message *)ptr;
    const struct Sample *smp = smps[i];

    if (ptr + MSG_LEN(smp->length) > buf + len)
      break;

    ret = msg_from_sample(msg, smp, smp->signals, source_index);
    if (ret)
      return ret;

    if (web) {
      // TODO: convert to little endian
    } else
      msg_hton(msg);

    ptr += MSG_LEN(smp->length);
  }

  if (wbytes)
    *wbytes = ptr - buf;

  return i;
}

int VillasBinaryFormat::sscan(const char *buf, size_t len, size_t *rbytes,
                              struct Sample *const smps[], unsigned cnt) {
  int ret, values;
  unsigned i, j;
  const char *ptr = buf;
  uint8_t sid; // source_index

  if (len % 4 != 0)
    return -1; // Packet size is invalid: Must be multiple of 4 bytes

  for (i = 0, j = 0; i < cnt; i++) {
    struct Message *msg = (struct Message *)ptr;
    struct Sample *smp = smps[j];

    smp->signals = signals;

    // Complete buffer has been parsed
    if (ptr == buf + len)
      break;

    // Check if header is still in buffer bounaries
    if (ptr + sizeof(struct Message) > buf + len)
      return -2; // Invalid msg received

    values = web ? msg->length : ntohs(msg->length);

    // Check if remainder of message is in buffer boundaries
    if (ptr + MSG_LEN(values) > buf + len)
      return -3; // Invalid msg receive

    if (web) {
      // TODO: convert from little endian
    } else
      msg_ntoh(msg);

    ret = msg_to_sample(msg, smp, signals, &sid);
    if (ret)
      return ret; // Invalid msg received

    if (validate_source_index && sid != source_index) {
      // source index mismatch: we skip this sample
    } else
      j++;

    ptr += MSG_LEN(smp->length);
  }

  if (rbytes)
    *rbytes = ptr - buf;

  return j;
}

void VillasBinaryFormat::parse(json_t *json) {
  int ret;
  json_error_t err;
  int sid = -1;
  int vsi = -1;

  ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b }", "source_index", &sid,
                       "validate_source_index", &vsi);
  if (ret)
    throw ConfigError(json, err, "node-config-format-villas-binary",
                      "Failed to parse format configuration");

  if (vsi >= 0)
    validate_source_index = vsi != 0;

  if (sid >= 0)
    source_index = sid;

  Format::parse(json);
}

// Register formats
static VillasBinaryFormatPlugin<false> p1;
static VillasBinaryFormatPlugin<true> p2;
