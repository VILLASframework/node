/* Node type: IEC 61850-9-2 (Sampled Values).
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <libiec61850/sv_publisher.h>
#include <libiec61850/sv_subscriber.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/iec61850_sv.hpp>
#include <villas/sample.hpp>
#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>
#include <villas/utils.hpp>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

static unsigned iec61850_sv_setup_asdu(NodeCompat *n, struct Sample *smp) {
  auto *i = n->getData<struct iec61850_sv>();

  unsigned new_length = MIN(list_length(&i->out.signals), smp->length);

  SVPublisher_ASDU_resetBuffer(i->out.asdu);
  SVPublisher_ASDU_enableRefrTm(i->out.asdu);

  for (unsigned k = 0; k < new_length; k++) {
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

  // Recalculate payload length
  SVPublisher_setupComplete(i->out.publisher);

  return new_length;
}

static void iec61850_sv_listener(SVSubscriber subscriber, void *ctx,
                                 SVSubscriber_ASDU asdu) {
  auto *n = (NodeCompat *)ctx;
  auto *i = n->getData<struct iec61850_sv>();
  struct Sample *smp;

  const char *sv_id = SVSubscriber_ASDU_getSvId(asdu);
  int smp_cnt = SVSubscriber_ASDU_getSmpCnt(asdu);
  size_t data_size = (size_t)SVSubscriber_ASDU_getDataSize(asdu);

  n->logger->debug("Received sample: sv_id={}, smp_cnt={}", sv_id, smp_cnt);

  smp = sample_alloc(&i->in.pool);
  if (!smp) {
    n->logger->warn("Pool underrun in subscriber");
    return;
  }

  smp->sequence = smp_cnt;
  smp->flags = (int)SampleFlags::HAS_SEQUENCE | (int)SampleFlags::HAS_DATA;
  smp->length = 0;
  smp->signals = n->getInputSignals(false);

  if (SVSubscriber_ASDU_hasRefrTm(asdu)) {
    uint64_t t = SVSubscriber_ASDU_getRefrTmAsNs(asdu);

    smp->ts.origin.tv_sec = t / 1000000000;
    smp->ts.origin.tv_nsec = t % 1000000000;
    smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;
  }

  for (size_t j = 0, off = 0;
       j < MIN(list_length(&i->in.signals), smp->capacity) &&
       off < MIN(i->in.total_size, data_size);
       j++) {
    struct iec61850_type_descriptor *td =
        (struct iec61850_type_descriptor *)list_at(&i->in.signals, j);
    auto sig = smp->signals->getByIndex(j);
    if (!sig)
      continue;

    switch (td->iec_type) {
    case IEC61850Type::INT8:
      smp->data[j].i = SVSubscriber_ASDU_getINT8(asdu, off);
      break;

    case IEC61850Type::INT16:
      smp->data[j].i = SVSubscriber_ASDU_getINT16(asdu, off);
      break;

    case IEC61850Type::INT32:
      smp->data[j].i = SVSubscriber_ASDU_getINT32(asdu, off);
      break;

    case IEC61850Type::INT8U:
      smp->data[j].i = SVSubscriber_ASDU_getINT8U(asdu, off);
      break;

    case IEC61850Type::INT16U:
      smp->data[j].i = SVSubscriber_ASDU_getINT16U(asdu, off);
      break;

    case IEC61850Type::INT32U:
      smp->data[j].i = SVSubscriber_ASDU_getINT32U(asdu, off);
      break;

    case IEC61850Type::FLOAT32:
      smp->data[j].f = SVSubscriber_ASDU_getFLOAT32(asdu, off);
      break;

    case IEC61850Type::FLOAT64:
      smp->data[j].f = SVSubscriber_ASDU_getFLOAT64(asdu, off);
      break;

    default: {
    }
    }

    off += td->size;
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
  const char *sv_id = nullptr;
  const char *smp_mod = nullptr;
  const char *smp_synch = nullptr;

  int check_dst_address = -1;

  json_t *json_in = nullptr;
  json_t *json_out = nullptr;
  json_t *json_signals = nullptr;
  json_t *json_vlan = nullptr;
  json_error_t err;

  ret =
      json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s: s, s?: i, s?: s }",
                     "out", &json_out, "in", &json_in, "interface", &interface,
                     "app_id", &i->app_id, "dst_address", &dst_address);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-sv");

  if (interface)
    i->interface = strdup(interface);

  if (dst_address) {
    struct ether_addr *addr = ether_aton_r(dst_address, &i->dst_address);
    if (addr == nullptr)
      throw ConfigError(json, "node-config-node-iec61850-sv-dst-address",
                        "Invalid setting 'dst_address': {}", dst_address);
  }

  if (json_out) {
    i->out.enabled = true;

    ret = json_unpack_ex(
        json_out, &err, 0, "{ s: o, s: s, s?: i, s?: s, s?: i, s?: s, s?: o }",
        "signals", &json_signals, "sv_id", &sv_id, "conf_rev", &i->out.conf_rev,
        "smp_mod", &smp_mod, "smp_rate", &i->out.smp_rate, "smp_synch",
        &smp_synch, "vlan", &json_vlan);
    if (ret)
      throw ConfigError(json_out, err, "node-config-node-iec61850-sv-out");

    if (json_vlan != nullptr) {
      int enabled = -1;
      json_unpack_ex(json_vlan, &err, 0, "{ s?: b, s?: i, s?: i }", "enabled",
                     &enabled, "id", &i->out.vlan.id, "priority",
                     &i->out.vlan.priority);

      if (enabled >= 0) {
        i->out.vlan.enabled = enabled > 0;
      }
    }

    if (smp_mod) {
      if (!strcmp(smp_mod, "per_nominal_period"))
        i->out.smp_mod = IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD;
      else if (!strcmp(smp_mod, "samples_per_second"))
        i->out.smp_mod = IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND;
      else if (!strcmp(smp_mod, "seconds_per_sample"))
        i->out.smp_mod = IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE;
      else
        throw ConfigError(json_out, "node-config-node-iec61850-sv-out-smp-mod",
                          "Invalid value '{}' for setting 'smp_mod'", smp_mod);
    }

    if (smp_synch) {
      if (!strcmp(smp_synch, "not_synchronized"))
        i->out.smp_synch = IEC61850_SV_SMPSYNC_NOT_SYNCHRONIZED;
      else if (!strcmp(smp_synch, "local_clock"))
        i->out.smp_synch = IEC61850_SV_SMPSYNC_SYNCED_UNSPEC_LOCAL_CLOCK;
      else if (!strcmp(smp_synch, "global_clock"))
        i->out.smp_synch = IEC61850_SV_SMPSYNC_SYNCED_GLOBAL_CLOCK;
      else
        throw ConfigError(
            json_out, "node-config-node-iec61850-sv-out-smp-synch",
            "Invalid value '{}' for setting 'smp_synch'", smp_synch);
    }

    i->out.sv_id = sv_id ? strdup(sv_id) : nullptr;

    ret = iec61850_parse_signals(json_signals, &i->out.signals,
                                 n->getOutputSignals());
    if (ret <= 0)
      throw ConfigError(json_signals,
                        "node-config-node-iec61850-sv-out-signals",
                        "Failed to parse setting 'signals'");
  }

  if (json_in) {
    i->in.enabled = true;

    json_signals = nullptr;
    ret =
        json_unpack_ex(json_in, &err, 0, "{ s: o, s?: b }", "signals",
                       &json_signals, "check_dst_address", &check_dst_address);
    if (ret)
      throw ConfigError(json_in, err, "node-config-node-iec61850-in");

    if (check_dst_address > 0)
      i->in.check_dst_address = check_dst_address > 0;

    ret = iec61850_parse_signals(json_signals, &i->in.signals,
                                 n->getInputSignals(false));
    if (ret <= 0)
      throw ConfigError(json_signals, "node-config-node-iec61850-sv-in-signals",
                        "Failed to parse setting 'signals'");

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
    strcatf(&buf, ", out.sv_id=%s, out.conf_rev=%d", i->out.sv_id,
            i->out.conf_rev);

    if (i->out.vlan.enabled) {
      strcatf(&buf,
              ", out.vlan.enabled=true, out.vlan.priority=%d, pub.vlan.id=%#x",
              i->out.vlan.priority, i->out.vlan.id);
    }

    auto output_signals = n->getOutputSignals();
    if (output_signals)
      strcatf(&buf, ", out.#signals=%zu", output_signals->size());
  }

  // Subscriber part
  if (i->in.enabled)
    strcatf(&buf, ", in.#signals=%zu", list_length(&i->in.signals));

  return buf;
}

int villas::node::iec61850_sv_start(NodeCompat *n) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  // Initialize publisher
  if (i->out.enabled) {
    CommParameters comm_params = {.vlanPriority = uint8_t(i->out.vlan.priority),
                                  .vlanId = uint16_t(i->out.vlan.id),
                                  .appId = uint16_t(i->app_id)};
    memcpy(comm_params.dstAddress, i->dst_address.ether_addr_octet, 6);

    i->out.publisher =
        SVPublisher_createEx(&comm_params, i->interface, i->out.vlan.enabled);
    if (i->out.publisher == nullptr)
      throw RuntimeError("Failed to create SV publisher");

    i->out.asdu =
        SVPublisher_addASDU(i->out.publisher, i->out.sv_id,
                            n->getNameShort().c_str(), i->out.conf_rev);
  }

  // Start subscriber
  if (i->in.enabled) {
    struct iec61850_receiver *r =
        iec61850_receiver_create(iec61850_receiver::Type::SAMPLED_VALUES,
                                 i->interface, i->in.check_dst_address);

    i->in.receiver = r->sv;
    i->in.subscriber = SVSubscriber_create(nullptr, i->app_id);

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

int villas::node::iec61850_sv_init(NodeCompat *n) {
  auto *i = n->getData<struct iec61850_sv>();

  uint8_t tmp[] = CONFIG_SV_DEFAULT_DST_ADDRESS;
  memcpy(i->dst_address.ether_addr_octet, tmp,
         sizeof(i->dst_address.ether_addr_octet));

  i->app_id = CONFIG_SV_DEFAULT_APPID;

  i->in.enabled = false;

  i->out.enabled = false;
  i->out.smp_mod = -1;   // Do not set smp_mod
  i->out.smp_synch = -1; // Do not set smp_synch
  i->out.smp_rate = -1;  // Do not set smp_rate
  i->out.conf_rev = 1;

  i->out.vlan.enabled = true;
  i->out.vlan.priority = CONFIG_SV_DEFAULT_PRIORITY;
  i->out.vlan.id = CONFIG_SV_DEFAULT_VLAN_ID;

  i->out.asdu_length = 0;

  return 0;
}

int villas::node::iec61850_sv_destroy(NodeCompat *n) {
  int ret;
  auto *i = n->getData<struct iec61850_sv>();

  // Deinitialize publisher
  if (i->out.enabled && i->out.publisher)
    SVPublisher_destroy(i->out.publisher);

  // Deinitialize subscriber
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
    auto *smp = smps[j];

    unsigned asdu_length = MIN(smp->length, list_length(&i->out.signals));
    if (i->out.asdu_length != asdu_length)
      i->out.asdu_length = iec61850_sv_setup_asdu(n, smp);

    if (i->out.smp_mod >= 0)
      SVPublisher_ASDU_setSmpMod(i->out.asdu, i->out.smp_mod);

    if (i->out.smp_synch >= 0)
      SVPublisher_ASDU_setSmpSynch(i->out.asdu, i->out.smp_synch);

    if (i->out.smp_rate >= 0)
      SVPublisher_ASDU_setSmpRate(i->out.asdu, i->out.smp_rate);

    unsigned off = 0;
    for (unsigned k = 0; k < i->out.asdu_length; k++) {
      auto *td = (struct iec61850_type_descriptor *)list_at(&i->out.signals, k);
      auto sig = smp->signals->getByIndex(k);

      SignalData data;

      switch (td->iec_type) {
      case IEC61850Type::INT8:
      case IEC61850Type::INT32:
      case IEC61850Type::INT64:
        data = smp->data[k].cast(sig->type, SignalType::INTEGER);
        break;

      case IEC61850Type::FLOAT32:
      case IEC61850Type::FLOAT64:
        data = smp->data[k].cast(sig->type, SignalType::FLOAT);
        break;

      default: {
      }
      }

      switch (td->iec_type) {
      case IEC61850Type::BOOLEAN:
        SVPublisher_ASDU_setINT8(i->out.asdu, off, data.b);
        break;

      case IEC61850Type::INT8:
        SVPublisher_ASDU_setINT8(i->out.asdu, off, data.i);
        break;

      case IEC61850Type::INT32:
        SVPublisher_ASDU_setINT32(i->out.asdu, off, data.i);
        break;

      case IEC61850Type::INT64:
        SVPublisher_ASDU_setINT64(i->out.asdu, off, data.i);
        break;

      case IEC61850Type::FLOAT32:
        SVPublisher_ASDU_setFLOAT(i->out.asdu, off, data.f);
        break;

      case IEC61850Type::FLOAT64:
        SVPublisher_ASDU_setFLOAT64(i->out.asdu, off, data.f);
        break;

      default: {
      }
      }

      off += td->size;
    }

    if (smp->flags & (int)SampleFlags::HAS_SEQUENCE)
      SVPublisher_ASDU_setSmpCnt(i->out.asdu, smp->sequence);

    if (smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) {
      uint64_t t = smp->ts.origin.tv_sec * 1000000000 + smp->ts.origin.tv_nsec;

      SVPublisher_ASDU_setRefrTmNs(i->out.asdu, t);
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
  p.init = iec61850_sv_init;
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
