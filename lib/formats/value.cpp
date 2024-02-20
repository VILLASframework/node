/* Bare text values.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/formats/value.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>

using namespace villas::node;

int ValueFormat::sprint(char *buf, size_t len, size_t *wbytes,
                        const struct Sample *const smps[], unsigned cnt) {
  unsigned i;
  size_t off = 0;
  const struct Sample *smp = smps[0];

  assert(cnt == 1);
  assert(smp->length <= 1);

  buf[0] = '\0';

  for (i = 0; i < smp->length; i++) {
    auto sig = smp->signals->getByIndex(i);
    if (!sig)
      return -1;

    off += smp->data[i].printString(sig->type, buf, len, real_precision);
    off += snprintf(buf + off, len - off, "\n");
  }

  if (wbytes)
    *wbytes = off;

  return i;
}

int ValueFormat::sscan(const char *buf, size_t len, size_t *rbytes,
                       struct Sample *const smps[], unsigned cnt) {
  unsigned i = 0, ret;
  struct Sample *smp = smps[0];

  const char *ptr = buf;
  char *end;

  assert(cnt == 1);

  printf("Reading: %s", buf);

  if (smp->capacity >= 1) {
    auto sig = signals->getByIndex(i);
    if (!sig)
      return -1;

    ret = smp->data[i].parseString(sig->type, ptr, &end);
    if (ret || end == ptr) // There are no valid values anymore.
      goto out;

    i++;
    ptr = end;
  }

out:
  smp->flags = 0;
  smp->signals = signals;
  smp->length = i;
  if (smp->length > 0)
    smp->flags |= (int)SampleFlags::HAS_DATA;

  if (rbytes)
    *rbytes = ptr - buf;

  return i;
}

// Register format
static char n[] = "value";
static char d[] = "A bare text value without any headers";
static FormatPlugin<ValueFormat, n, d, (int)SampleFlags::HAS_DATA> p;
