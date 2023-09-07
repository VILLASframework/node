/* Protobuf IO format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/formats/protobuf.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/utils.hpp>

using namespace villas::node;

enum SignalType ProtobufFormat::detect(const Villas__Node__Value *val) {
  switch (val->value_case) {
  case VILLAS__NODE__VALUE__VALUE_F:
    return SignalType::FLOAT;

  case VILLAS__NODE__VALUE__VALUE_I:
    return SignalType::INTEGER;

  case VILLAS__NODE__VALUE__VALUE_B:
    return SignalType::BOOLEAN;

  case VILLAS__NODE__VALUE__VALUE_Z:
    return SignalType::COMPLEX;

  case VILLAS__NODE__VALUE__VALUE__NOT_SET:
  default:
    return SignalType::INVALID;
  }
}

int ProtobufFormat::sprint(char *buf, size_t len, size_t *wbytes,
                           const struct Sample *const smps[], unsigned cnt) {
  unsigned psz;

  auto *pb_msg = new Villas__Node__Message;
  if (!pb_msg)
    throw MemoryAllocationError();

  villas__node__message__init(pb_msg);

  pb_msg->n_samples = cnt;
  pb_msg->samples = new Villas__Node__Sample *[pb_msg->n_samples];
  if (!pb_msg->samples)
    throw MemoryAllocationError();

  for (unsigned i = 0; i < pb_msg->n_samples; i++) {
    Villas__Node__Sample *pb_smp = pb_msg->samples[i] =
        new Villas__Node__Sample;
    if (!pb_msg->samples[i])
      throw MemoryAllocationError();

    villas__node__sample__init(pb_smp);

    const struct Sample *smp = smps[i];

    pb_smp->type = VILLAS__NODE__SAMPLE__TYPE__DATA;

    if (flags & smp->flags & (int)SampleFlags::HAS_SEQUENCE) {
      pb_smp->has_sequence = 1;
      pb_smp->sequence = smp->sequence;
    }

    if (flags & smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) {
      pb_smp->timestamp = new Villas__Node__Timestamp;
      if (!pb_smp->timestamp)
        throw MemoryAllocationError();

      villas__node__timestamp__init(pb_smp->timestamp);

      pb_smp->timestamp->sec = smp->ts.origin.tv_sec;
      pb_smp->timestamp->nsec = smp->ts.origin.tv_nsec;
    }

    pb_smp->n_values = smp->length;
    pb_smp->values = new Villas__Node__Value *[pb_smp->n_values];
    if (!pb_smp->values)
      throw MemoryAllocationError();

    for (unsigned j = 0; j < pb_smp->n_values; j++) {
      Villas__Node__Value *pb_val = pb_smp->values[j] = new Villas__Node__Value;
      if (!pb_val)
        throw MemoryAllocationError();

      villas__node__value__init(pb_val);

      enum SignalType fmt = sample_format(smp, j);
      switch (fmt) {
      case SignalType::FLOAT:
        pb_val->value_case = VILLAS__NODE__VALUE__VALUE_F;
        pb_val->f = smp->data[j].f;
        break;

      case SignalType::INTEGER:
        pb_val->value_case = VILLAS__NODE__VALUE__VALUE_I;
        pb_val->i = smp->data[j].i;
        break;

      case SignalType::BOOLEAN:
        pb_val->value_case = VILLAS__NODE__VALUE__VALUE_B;
        pb_val->b = smp->data[j].b;
        break;

      case SignalType::COMPLEX:
        pb_val->value_case = VILLAS__NODE__VALUE__VALUE_Z;
        pb_val->z = new Villas__Node__Complex;
        if (!pb_val->z)
          throw MemoryAllocationError();

        villas__node__complex__init(pb_val->z);

        pb_val->z->real = std::real(smp->data[j].z);
        pb_val->z->imag = std::imag(smp->data[j].z);
        break;

      case SignalType::INVALID:
        pb_val->value_case = VILLAS__NODE__VALUE__VALUE__NOT_SET;
        break;
      }
    }
  }

  psz = villas__node__message__get_packed_size(pb_msg);

  if (psz > len)
    goto out;

  villas__node__message__pack(pb_msg, (uint8_t *)buf);
  villas__node__message__free_unpacked(pb_msg, nullptr);

  *wbytes = psz;

  return cnt;

out:
  villas__node__message__free_unpacked(pb_msg, nullptr);

  return -1;
}

int ProtobufFormat::sscan(const char *buf, size_t len, size_t *rbytes,
                          struct Sample *const smps[], unsigned cnt) {
  unsigned i, j;
  Villas__Node__Message *pb_msg;

  pb_msg = villas__node__message__unpack(nullptr, len, (uint8_t *)buf);
  if (!pb_msg)
    return -1;

  for (i = 0; i < MIN(pb_msg->n_samples, cnt); i++) {
    struct Sample *smp = smps[i];
    Villas__Node__Sample *pb_smp = pb_msg->samples[i];

    smp->flags = 0;
    smp->signals = signals;

    if (pb_smp->type != VILLAS__NODE__SAMPLE__TYPE__DATA)
      throw RuntimeError("Parsed non supported message type. Skipping");

    if (pb_smp->has_sequence) {
      smp->flags |= (int)SampleFlags::HAS_SEQUENCE;
      smp->sequence = pb_smp->sequence;
    }

    if (pb_smp->timestamp) {
      smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;
      smp->ts.origin.tv_sec = pb_smp->timestamp->sec;
      smp->ts.origin.tv_nsec = pb_smp->timestamp->nsec;
    }

    for (j = 0; j < MIN(pb_smp->n_values, smp->capacity); j++) {
      Villas__Node__Value *pb_val = pb_smp->values[j];

      enum SignalType fmt = detect(pb_val);

      auto sig = smp->signals->getByIndex(j);
      if (!sig)
        return -1;

      if (sig->type != fmt)
        throw RuntimeError("Received invalid data type in Protobuf payload: "
                           "Received {}, expected {} for signal {} (index {}).",
                           signalTypeToString(fmt),
                           signalTypeToString(sig->type), sig->name, i);

      switch (sig->type) {
      case SignalType::FLOAT:
        smp->data[j].f = pb_val->f;
        break;

      case SignalType::INTEGER:
        smp->data[j].i = pb_val->i;
        break;

      case SignalType::BOOLEAN:
        smp->data[j].b = pb_val->b;
        break;

      case SignalType::COMPLEX:
        smp->data[j].z = std::complex<float>(pb_val->z->real, pb_val->z->imag);
        break;

      default: {
      }
      }
    }

    if (pb_smp->n_values > 0)
      smp->flags |= (int)SampleFlags::HAS_DATA;

    smp->length = j;
  }

  if (rbytes)
    *rbytes = villas__node__message__get_packed_size(pb_msg);

  villas__node__message__free_unpacked(pb_msg, nullptr);

  return i;
}

// Register format
static char n[] = "protobuf";
static char d[] = "Google Protobuf";
static FormatPlugin<ProtobufFormat, n, d,
                    (int)SampleFlags::HAS_TS_ORIGIN |
                        (int)SampleFlags::HAS_SEQUENCE |
                        (int)SampleFlags::HAS_DATA>
    p;
