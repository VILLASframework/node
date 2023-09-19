/* The internal datastructure for a sample of simulation data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include <cinttypes>
#include <cstring>

#include <openssl/crypto.h>
#include <villas/formats/villas_human.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas::node;

size_t VILLASHumanFormat::sprintLine(char *buf, size_t len,
                                     const struct Sample *smp) {
  size_t off = 0;

  if (flags & (int)SampleFlags::HAS_TS_ORIGIN) {
    if (smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) {
      off += snprintf(buf + off, len - off, "%llu",
                      (unsigned long long)smp->ts.origin.tv_sec);
      off += snprintf(buf + off, len - off, ".%09llu",
                      (unsigned long long)smp->ts.origin.tv_nsec);
    } else
      off += snprintf(buf + off, len - off, "0.0");
  }

  if (flags & (int)SampleFlags::HAS_OFFSET) {
    if (smp->flags & (int)SampleFlags::HAS_TS_RECEIVED)
      off += snprintf(buf + off, len - off, "%+e",
                      time_delta(&smp->ts.origin, &smp->ts.received));
  }

  if (flags & (int)SampleFlags::HAS_SEQUENCE) {
    if (smp->flags & (int)SampleFlags::HAS_SEQUENCE)
      off += snprintf(buf + off, len - off, "(%" PRIu64 ")", smp->sequence);
  }

  if (flags & (int)SampleFlags::NEW_FRAME) {
    if (smp->flags & (int)SampleFlags::NEW_FRAME)
      off += snprintf(buf + off, len - off, "F");
  }

  if (flags & (int)SampleFlags::HAS_DATA) {
    for (unsigned i = 0; i < smp->length; i++) {
      auto sig = smp->signals->getByIndex(i);
      if (!sig)
        break;

      off += snprintf(buf + off, len - off, "\t");
      off += smp->data[i].printString(sig->type, buf + off, len - off,
                                      real_precision);
    }
  }

  off += snprintf(buf + off, len - off, "%c", delimiter);

  return off;
}

size_t VILLASHumanFormat::sscanLine(const char *buf, size_t len,
                                    struct Sample *smp) {
  int ret;
  char *end;
  const char *ptr = buf;

  double offset = 0;

  smp->flags = 0;
  smp->signals = signals;

  /* Format: Seconds.NanoSeconds+Offset(SequenceNumber)Flags Value1 Value2 ...
	 * RegEx: (\d+(?:\.\d+)?)([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?(F)?
	 *
	 * Please note that only the seconds and at least one value are mandatory
	 */

  // Mandatory: seconds
  smp->ts.origin.tv_sec = (uint32_t)strtoul(ptr, &end, 10);
  if (ptr == end || *end == delimiter)
    return -1;

  smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;

  // Optional: nano seconds
  if (*end == '.') {
    ptr = end + 1;

    smp->ts.origin.tv_nsec = (uint32_t)strtoul(ptr, &end, 10);
    if (ptr == end)
      return -3;
  } else
    smp->ts.origin.tv_nsec = 0;

  // Optional: offset / delay
  if (*end == '+' || *end == '-') {
    ptr = end;

    offset = strtof(ptr, &end); // offset is ignored for now
    if (ptr != end)
      smp->flags |= (int)SampleFlags::HAS_OFFSET;
    else
      return -4;
  }

  // Optional: sequence
  if (*end == '(') {
    ptr = end + 1;

    smp->sequence = strtoul(ptr, &end, 10);
    if (ptr != end)
      smp->flags |= (int)SampleFlags::HAS_SEQUENCE;
    else
      return -5;

    if (*end == ')')
      end++;
  }

  // Optional: NEW_FRAME flag
  if (*end == 'F') {
    smp->flags |= (int)SampleFlags::NEW_FRAME;
    end++;
  }

  unsigned i;
  for (ptr = end + 1, i = 0; i < smp->capacity; ptr = end + 1, i++) {
    if (*end == delimiter)
      goto out;

    auto sig = signals->getByIndex(i);
    if (!sig)
      goto out;

    ret = smp->data[i].parseString(sig->type, ptr, &end);
    if (ret || end == ptr) // There are no valid values anymore.
      goto out;
  }

out:
  if (*end == delimiter)
    end++;

  smp->length = i;
  if (smp->length > 0)
    smp->flags |= (int)SampleFlags::HAS_DATA;

  if (smp->flags & (int)SampleFlags::HAS_OFFSET) {
    struct timespec off = time_from_double(offset);
    smp->ts.received = time_add(&smp->ts.origin, &off);

    smp->flags |= (int)SampleFlags::HAS_TS_RECEIVED;
  }

  return end - buf;
}

void VILLASHumanFormat::header(FILE *f, const SignalList::Ptr sigs) {
  // Abort if we are not supposed to, or have already printed the header
  if (!print_header || header_printed)
    return;

  fprintf(f, "# ");

  if (flags & (int)SampleFlags::HAS_TS_ORIGIN)
    fprintf(f, "seconds.nanoseconds");

  if (flags & (int)SampleFlags::HAS_OFFSET)
    fprintf(f, "+offset");

  if (flags & (int)SampleFlags::HAS_SEQUENCE)
    fprintf(f, "(sequence)");

  if (flags & (int)SampleFlags::HAS_DATA) {
    for (unsigned i = 0; i < sigs->size(); i++) {
      auto sig = sigs->getByIndex(i);
      if (!sig)
        break;

      if (!sig->name.empty())
        fprintf(f, "\t%s", sig->name.c_str());
      else
        fprintf(f, "\tsignal%u", i);

      if (!sig->unit.empty())
        fprintf(f, "[%s]", sig->unit.c_str());
    }
  }

  fprintf(f, "%c", delimiter);

  LineFormat::header(f, sigs);
}

// Register format
static char n[] = "villas.human";
static char d[] = "VILLAS human readable format";
static LineFormatPlugin<
    VILLASHumanFormat, n, d,
    (int)SampleFlags::HAS_TS_ORIGIN | (int)SampleFlags::HAS_SEQUENCE |
        (int)SampleFlags::HAS_DATA | (int)SampleFlags::NEW_FRAME,
    '\n'>
    p;
