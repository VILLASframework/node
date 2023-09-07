/* A custom format for OPAL-RTs AsyncIP example.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <endian.h>

#include <villas/exceptions.hpp>
#include <villas/formats/opal_asyncip.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;

int OpalAsyncIPFormat::sprint(char *buf, size_t len, size_t *wbytes,
                              const struct Sample *const smps[], unsigned cnt) {
  unsigned i;
  auto *ptr = buf;
  ssize_t slen = len;

  for (i = 0; i < cnt && ptr - buf < slen; i++) {
    auto *pl = (struct Payload *)ptr;
    auto *smp = smps[i];

    auto wlen = smp->length * sizeof(double) + sizeof(struct Payload);
    if (wlen > len)
      return -1;

    pl->dev_id = htole16(dev_id);
    pl->msg_id = htole32(smp->sequence);
    pl->msg_len = htole16(smp->length * sizeof(double));

    if (smp->length > MAXSIZE)
      logger->warn("Can not sent more then {} signals via opal.asyncip format. "
                   "We only send the first {}..",
                   MAXSIZE, MAXSIZE);

    for (unsigned j = 0; j < MIN(MAXSIZE, smp->length); j++) {
      auto sig = smp->signals->getByIndex(j);
      auto d = smp->data[j];

      d = d.cast(sig->type, SignalType::FLOAT);
      d.i = htole64(d.i);

      pl->data[j] = d.f;
    }

    ptr += wlen;
  }

  if (wbytes)
    *wbytes = ptr - buf;

  return i;
}

int OpalAsyncIPFormat::sscan(const char *buf, size_t len, size_t *rbytes,
                             struct Sample *const smps[], unsigned cnt) {
  unsigned i;
  auto *ptr = buf;

  if (len % 8 != 0)
    return -1; // Packet size is invalid: Must be multiple of 8 bytes

  for (i = 0; i < cnt && ptr - buf + sizeof(struct Payload) < len; i++) {
    auto *pl = (struct Payload *)ptr;
    auto *smp = smps[i];

    auto rlen = le16toh(pl->msg_len);
    if (len < ptr - buf + rlen + sizeof(struct Payload))
      return -2;

    smp->sequence = le32toh(pl->msg_id);
    smp->length = rlen / sizeof(double);
    smp->flags = (int)SampleFlags::HAS_SEQUENCE | (int)SampleFlags::HAS_DATA;
    smp->signals = signals;

    for (unsigned j = 0; j < MIN(smp->length, smp->capacity); j++) {
      auto sig = signals->getByIndex(j);

      SignalData d;
      d.f = pl->data[j];
      d.i = le64toh(d.i);

      smp->data[j] = d.cast(SignalType::FLOAT, sig->type);
    }

    ptr += rlen + sizeof(struct Payload);
  }

  if (rbytes)
    *rbytes = ptr - buf;

  return i;
}

void OpalAsyncIPFormat::parse(json_t *json) {
  int ret;
  json_error_t err;
  int did = -1;

  ret = json_unpack_ex(json, &err, 0, "{ s?: i }", "dev_id", &did);
  if (ret)
    throw ConfigError(json, err, "node-config-format-opal-asyncip",
                      "Failed to parse format configuration");

  if (did >= 0)
    dev_id = did;

  Format::parse(json);
}

static OpalAsyncIPFormatPlugin p;
