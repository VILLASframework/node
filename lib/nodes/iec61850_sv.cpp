/* Node type: IEC 61850-9-2 (Sampled Values).
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/iec61850_sv.hpp>
#include <villas/utils.hpp>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

static void iec61850_sv_listener(SVSubscriber subscriber, void *ctx,
                                 SVSubscriber_ASDU asdu) {
  auto *n = (NodeCompat *)ctx;
  auto *i = n->getData<struct iec61850_sv>();
  struct Sample *smp;

  const char *svid = SVSubscriber_ASDU_getSvId(asdu);
  int smpcnt = SVSubscriber_ASDU_getSmpCnt(asdu);
  int confrev = SVSubscriber_ASDU_getConfRev(asdu);
  int sz;

  n->logger->debug("Received SV: svid={}, smpcnt={}, confrev={}", svid, smpcnt,
                   confrev);

  sz = SVSubscriber_ASDU_getDataSize(asdu);
  if (sz < i->in.total_size) {
    n->logger->warn("Received truncated ASDU: size={}, expected={}",
                    SVSubscriber_ASDU_getDataSize(asdu), i->in.total_size);
    return;
  }

  /* Access to the data requires a priori knowledge of the data set.
	 * For this example we assume a data set consisting of FLOAT32 values.
	 * A FLOAT32 value is encoded as 4 bytes. You can find the first FLOAT32
	 * value at byte position 0, the second value at byte position 4, the third
	 * value at byte position 8, and so on.
	 *
	 * To prevent damages due configuration, please check the length of the
	 * data block of the SV message before accessing the data.
	 */

  smp = sample_alloc(&i->in.pool);
  if (!smp) {
    n->logger->warn("Pool underrun in subscriber");
    return;
  }

  smp->sequence = smpcnt;
  smp->flags = (int)SampleFlags::HAS_SEQUENCE | (int)SampleFlags::HAS_DATA;
  smp->length = 0;
  smp->signals = n->getInputSignals(false);

  if (SVSubscriber_ASDU_hasRefrTm(asdu)) {
    uint64_t refrtm = SVSubscriber_ASDU_getRefrTmAsMs(asdu);

    smp->ts.origin.tv_sec = refrtm / 1000;
    smp->ts.origin.tv_nsec = (refrtm % 1000) * 1000000;
    smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;
  }

  unsigned offset = 0;
  for (size_t j = 0; j < list_length(&i->in.signals); j++) {
    struct iec61850_type_descriptor *td =
        (struct iec61850_type_descriptor *)list_at(&i->in.signals, j);
    auto sig = smp->signals->getByIndex(j);
    if (!sig)
      continue;

    switch (td->iec_type) {
    case IEC61850Type::INT8:
      smp->data[j].i = SVSubscriber_ASDU_getINT8(asdu, offset);
      break;

    case IEC61850Type::INT16:
      smp->data[j].i = SVSubscriber_ASDU_getINT16(asdu, offset);
      break;

    case IEC61850Type::INT32:
      smp->data[j].i = SVSubscriber_ASDU_getINT32(asdu, offset);
      break;

    case IEC61850Type::INT8U:
      smp->data[j].i = SVSubscriber_ASDU_getINT8U(asdu, offset);
      break;

    case IEC61850Type::INT16U:
      smp->data[j].i = SVSubscriber_ASDU_getINT16U(asdu, offset);
      break;

    case IEC61850Type::INT32U:
      smp->data[j].i = SVSubscriber_ASDU_getINT32U(asdu, offset);
      break;

    case IEC61850Type::FLOAT32:
      smp->data[j].f = SVSubscriber_ASDU_getFLOAT32(asdu, offset);
      break;

    case IEC61850Type::FLOAT64:
      smp->data[j].f = SVSubscriber_ASDU_getFLOAT64(asdu, offset);
      break;

    default: {
    }
    }

    offset += td->size;

    smp->length++;
  }

  int pushed __attribute__((unused));
  pushed = queue_signalled_push(&i->in.queue, smp);
}

int villas::node::iec61850_sv_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  const char *dst_address = nullptr;
  const char *interface = nullptr;
  const char *svid = nullptr;
  const char *smpmod = nullptr;

  json_t *json_in = nullptr;
  json_t *json_out = nullptr;
  json_t *json_signals = nullptr;
  json_error_t err;

  // Default values
  i->out.enabled = false;
  i->in.enabled = false;
  i->out.smpmod = -1;  // do not set smpmod
  i->out.smprate = -1; // do not set smpmod
  i->out.confrev = 1;
  i->out.vlan_priority = CONFIG_SV_DEFAULT_PRIORITY;
  i->out.vlan_id = CONFIG_SV_DEFAULT_VLAN_ID;

  i->app_id = CONFIG_SV_DEFAULT_APPID;

  uint8_t tmp[] = CONFIG_SV_DEFAULT_DST_ADDRESS;
  memcpy(i->dst_address.ether_addr_octet, tmp,
         sizeof(i->dst_address.ether_addr_octet));

  ret =
      json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s: s, s?: i, s?: s }",
                     "out", &json_out, "in", &json_in, "interface", &interface,
                     "app_id", &i->app_id, "dst_address", &dst_address);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-sv");

  if (interface)
    i->interface = strdup(interface);

  if (dst_address)
    ether_aton_r(dst_address, &i->dst_address);

  if (json_out) {
    i->out.enabled = true;

    ret = json_unpack_ex(
        json_out, &err, 0, "{ s: o, s: s, s?: i, s?: s, s?: i, s?: i, s?: i }",
        "signals", &json_signals, "svid", &svid, "confrev", &i->out.confrev,
        "smpmod", &smpmod, "smprate", &i->out.smprate, "vlan_id",
        &i->out.vlan_id, "vlan_priority", &i->out.vlan_priority);
    if (ret)
      throw ConfigError(json_out, err, "node-config-node-iec61850-sv-out");

    if (smpmod) {
      if (!strcmp(smpmod, "per_nominal_period"))
        i->out.smpmod = IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD;
      else if (!strcmp(smpmod, "samples_per_second"))
        i->out.smpmod = IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND;
      else if (!strcmp(smpmod, "seconds_per_sample"))
        i->out.smpmod = IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE;
      else
        throw RuntimeError("Invalid value '{}' for setting 'smpmod'", smpmod);
    }

    i->out.svid = svid ? strdup(svid) : nullptr;

    ret = iec61850_parse_signals(json_signals, &i->out.signals,
                                 n->getOutputSignals());
    if (ret <= 0)
      throw RuntimeError("Failed to parse setting 'signals'");

    i->out.total_size = ret;
  }

  if (json_in) {
    i->in.enabled = true;

    json_signals = nullptr;
    ret =
        json_unpack_ex(json_in, &err, 0, "{ s: o }", "signals", &json_signals);
    if (ret)
      throw ConfigError(json_in, err, "node-config-node-iec61850-in");

    ret = iec61850_parse_signals(json_signals, &i->in.signals,
                                 n->getInputSignals(false));
    if (ret <= 0)
      throw RuntimeError("Failed to parse setting 'signals'");

    i->in.total_size = ret;
  }

  return 0;
}

char *villas::node::iec61850_sv_print(NodeCompat *n) {
  char *buf;
  auto *i = n->getData<struct iec61850_sv>();

  buf = strf("interface=%s, app_id=%#x, dst_address=%s", i->interface,
             i->app_id, ether_ntoa(&i->dst_address));

  // Publisher part
  if (i->out.enabled) {
    strcatf(&buf,
            ", pub.svid=%s, pub.vlan_prio=%d, pub.vlan_id=%#x, pub.confrev=%d, "
            "pub.#fields=%zu",
            i->out.svid, i->out.vlan_priority, i->out.vlan_id, i->out.confrev,
            n->getOutputSignals()->size());
  }

  // Subscriber part
  if (i->in.enabled)
    strcatf(&buf, ", sub.#fields=%zu", list_length(&i->in.signals));

  return buf;
}

int villas::node::iec61850_sv_start(NodeCompat *n) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  // Initialize publisher
  if (i->out.enabled) {
    i->out.publisher = SVPublisher_create(nullptr, i->interface);
    i->out.asdu =
        SVPublisher_addASDU(i->out.publisher, i->out.svid,
                            n->getNameShort().c_str(), i->out.confrev);

    for (unsigned k = 0; k < list_length(&i->out.signals); k++) {
      struct iec61850_type_descriptor *td =
          (struct iec61850_type_descriptor *)list_at(&i->out.signals, k);

      switch (td->iec_type) {
      case IEC61850Type::INT8:
        SVPublisher_ASDU_addINT8(i->out.asdu);
        break;

      case IEC61850Type::INT32:
        SVPublisher_ASDU_addINT32(i->out.asdu);
        break;

      case IEC61850Type::FLOAT32:
        SVPublisher_ASDU_addFLOAT(i->out.asdu);
        break;

      case IEC61850Type::FLOAT64:
        SVPublisher_ASDU_addFLOAT64(i->out.asdu);
        break;

      default: {
      }
      }
    }

    if (i->out.smpmod >= 0)
      SVPublisher_ASDU_setSmpMod(i->out.asdu, i->out.smpmod);

    SVPublisher_ASDU_enableRefrTm(i->out.asdu);

    //		if (s->out.smprate >= 0)
    //			SV_ASDU_setSmpRate(i->out.asdu, i->out.smprate);

    // Start publisher
    SVPublisher_setupComplete(i->out.publisher);
  }

  // Start subscriber
  if (i->in.enabled) {
    struct iec61850_receiver *r = iec61850_receiver_create(
        iec61850_receiver::Type::SAMPLED_VALUES, i->interface);

    i->in.receiver = r->sv;
    i->in.subscriber =
        SVSubscriber_create(i->dst_address.ether_addr_octet, i->app_id);

    // Install a callback handler for the subscriber
    SVSubscriber_setListener(i->in.subscriber, iec61850_sv_listener, n);

    // Connect the subscriber to the receiver
    SVReceiver_addSubscriber(i->in.receiver, i->in.subscriber);

    // Initialize pool and queue to pass samples between threads
    ret = pool_init(&i->in.pool, 1024,
                    SAMPLE_LENGTH(n->getInputSignals(false)->size()));
    if (ret)
      return ret;

    ret = queue_signalled_init(&i->in.queue, 1024);
    if (ret)
      return ret;

    for (unsigned k = 0; k < list_length(&i->in.signals); k++) {
      struct iec61850_type_descriptor *td =
          (struct iec61850_type_descriptor *)list_at(&i->in.signals, k);
      auto sig = n->getInputSignals(false)->getByIndex(k);

      if (sig->type == SignalType::INVALID)
        sig->type = td->type;
      else if (sig->type != td->type)
        return -1;
    }
  }

  return 0;
}

int villas::node::iec61850_sv_stop(NodeCompat *n) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  if (i->in.enabled)
    SVReceiver_removeSubscriber(i->in.receiver, i->in.subscriber);

  ret = queue_signalled_close(&i->in.queue);
  if (ret)
    return ret;

  return 0;
}

int villas::node::iec61850_sv_destroy(NodeCompat *n) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  // Deinitialize publisher
  if (i->out.enabled && i->out.publisher)
    SVPublisher_destroy(i->out.publisher);

  // Deinitialise subscriber
  if (i->in.enabled) {
    ret = queue_signalled_destroy(&i->in.queue);
    if (ret)
      return ret;

    ret = pool_destroy(&i->in.pool);
    if (ret)
      return ret;
  }

  return 0;
}

int villas::node::iec61850_sv_read(NodeCompat *n, struct Sample *const smps[],
                                   unsigned cnt) {
  int pulled;
  auto *i = n->getData<struct iec61850_sv>();
  struct Sample *smpt[cnt];

  if (!i->in.enabled)
    return -1;

  pulled = queue_signalled_pull_many(&i->in.queue, (void **)smpt, cnt);

  sample_copy_many(smps, smpt, pulled);
  sample_decref_many(smpt, pulled);

  return pulled;
}

int villas::node::iec61850_sv_write(NodeCompat *n, struct Sample *const smps[],
                                    unsigned cnt) {
  auto *i = n->getData<struct iec61850_sv>();

  if (!i->out.enabled)
    return -1;

  for (unsigned j = 0; j < cnt; j++) {
    unsigned offset = 0;
    for (unsigned k = 0; k < MIN(smps[j]->length, list_length(&i->out.signals));
         k++) {
      struct iec61850_type_descriptor *td =
          (struct iec61850_type_descriptor *)list_at(&i->out.signals, k);

      int ival = 0;
      double fval = 0;

      switch (td->iec_type) {
      case IEC61850Type::INT8:
      case IEC61850Type::INT32:
        ival = sample_format(smps[j], k) == SignalType::FLOAT
                   ? smps[j]->data[k].f
                   : smps[j]->data[k].i;
        break;

      case IEC61850Type::FLOAT32:
      case IEC61850Type::FLOAT64:
        fval = sample_format(smps[j], k) == SignalType::FLOAT
                   ? smps[j]->data[k].f
                   : smps[j]->data[k].i;
        break;

      default: {
      }
      }

      switch (td->iec_type) {
      case IEC61850Type::INT8:
        SVPublisher_ASDU_setINT8(i->out.asdu, offset, ival);
        break;

      case IEC61850Type::INT32:
        SVPublisher_ASDU_setINT32(i->out.asdu, offset, ival);
        break;

      case IEC61850Type::FLOAT32:
        SVPublisher_ASDU_setFLOAT(i->out.asdu, offset, fval);
        break;

      case IEC61850Type::FLOAT64:
        SVPublisher_ASDU_setFLOAT64(i->out.asdu, offset, fval);
        break;

      default: {
      }
      }

      offset += td->size;
    }

    SVPublisher_ASDU_setSmpCnt(i->out.asdu, smps[j]->sequence);

    if (smps[j]->flags & (int)SampleFlags::HAS_TS_ORIGIN) {
      uint64_t refrtm = smps[j]->ts.origin.tv_sec * 1000 +
                        smps[j]->ts.origin.tv_nsec / 1000000;

      SVPublisher_ASDU_setRefrTm(i->out.asdu, refrtm);
    }

    SVPublisher_publish(i->out.publisher);
  }

  return cnt;
}

int villas::node::iec61850_sv_poll_fds(NodeCompat *n, int fds[]) {
  auto *i = n->getData<struct iec61850_sv>();

  fds[0] = queue_signalled_fd(&i->in.queue);

  return 1;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "iec61850-9-2";
  p.description = "IEC 61850-9-2 (Sampled Values)";
  p.vectorize = 0;
  p.size = sizeof(struct iec61850_sv);
  p.type.start = iec61850_type_start;
  p.type.stop = iec61850_type_stop;
  p.destroy = iec61850_sv_destroy;
  p.parse = iec61850_sv_parse;
  p.print = iec61850_sv_print;
  p.start = iec61850_sv_start;
  p.stop = iec61850_sv_stop;
  p.read = iec61850_sv_read;
  p.write = iec61850_sv_write;
  p.poll_fds = iec61850_sv_poll_fds;

  static NodeCompatFactory ncp(&p);
}
