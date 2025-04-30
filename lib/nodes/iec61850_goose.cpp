/* Node type: IEC 61850 - GOOSE.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>

#include <jansson.h>
#include <libiec61850/goose_publisher.h>
#include <libiec61850/goose_receiver.h>
#include <libiec61850/r_session.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/iec61850_goose.hpp>
#include <villas/sample.hpp>
#include <villas/signal_type.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec61850;

static std::optional<std::array<uint8_t, 6>> stringToMac(char *mac_string) {
  std::array<uint8_t, 6> mac;
  char *save;
  char *token = strtok_r(mac_string, ":", &save);

  for (auto &i : mac) {
    if (!token)
      return std::nullopt;

    i = static_cast<uint8_t>(strtol(token, NULL, 16));

    token = strtok_r(NULL, ":", &save);
  }

  return std::optional{mac};
}

MmsType GooseSignal::mmsType() const { return descriptor->mms_type; }

std::string const &GooseSignal::name() const { return descriptor->name; }

SignalType GooseSignal::signalType() const { return descriptor->signal_type; }

std::optional<GooseSignal> GooseSignal::fromMmsValue(MmsValue *mms_value) {
  auto mms_type = MmsValue_getType(mms_value);
  auto descriptor = lookupMmsType(mms_type);
  SignalData data;

  if (!descriptor)
    return std::nullopt;

  switch (mms_type) {
  case MmsType::MMS_BOOLEAN:
    data.b = MmsValue_getBoolean(mms_value);
    break;
  case MmsType::MMS_INTEGER:
    data.i = MmsValue_toInt64(mms_value);
    break;
  case MmsType::MMS_UNSIGNED:
    data.i = static_cast<int64_t>(MmsValue_toUint32(mms_value));
    break;
  case MmsType::MMS_BIT_STRING:
    data.i = static_cast<int64_t>(MmsValue_getBitStringAsInteger(mms_value));
    break;
  case MmsType::MMS_FLOAT:
    data.f = MmsValue_toDouble(mms_value);
    break;
  default:
    return std::nullopt;
  }

  return GooseSignal{descriptor.value(), data};
}

std::optional<GooseSignal>
GooseSignal::fromNameAndValue(char const *name, SignalData value,
                              std::optional<Meta> meta) {
  auto descriptor = lookupMmsTypeName(name);

  if (!descriptor)
    return std::nullopt;

  return GooseSignal{descriptor.value(), value, meta};
}

MmsValue *GooseSignal::toMmsValue() const {
  switch (descriptor->mms_type) {
  case MmsType::MMS_BOOLEAN:
    return MmsValue_newBoolean(signal_data.b);
  case MmsType::MMS_INTEGER:
    return newMmsInteger(signal_data.i, meta.size);
  case MmsType::MMS_UNSIGNED:
    return newMmsUnsigned(static_cast<uint64_t>(signal_data.i), meta.size);
  case MmsType::MMS_BIT_STRING:
    return newMmsBitString(static_cast<uint32_t>(signal_data.i), meta.size);
  case MmsType::MMS_FLOAT:
    return newMmsFloat(signal_data.f, meta.size);
  default:
    throw RuntimeError{"invalid mms type"};
  }
}

MmsValue *GooseSignal::newMmsBitString(uint32_t i, int size) {
  auto mms_bitstring = MmsValue_newBitString(size);

  MmsValue_setBitStringFromInteger(mms_bitstring, i);

  return mms_bitstring;
}

MmsValue *GooseSignal::newMmsInteger(int64_t i, int size) {
  auto mms_integer = MmsValue_newInteger(size);

  switch (size) {
  case 8:
    MmsValue_setInt8(mms_integer, static_cast<int8_t>(i));
    return mms_integer;
  case 16:
    MmsValue_setInt16(mms_integer, static_cast<int16_t>(i));
    return mms_integer;
  case 32:
    MmsValue_setInt32(mms_integer, static_cast<int32_t>(i));
    return mms_integer;
  case 64:
    MmsValue_setInt64(mms_integer, static_cast<int64_t>(i));
    return mms_integer;
  default:
    throw RuntimeError{"invalid mms integer size"};
  }
}

MmsValue *GooseSignal::newMmsUnsigned(uint64_t u, int size) {
  auto mms_unsigned = MmsValue_newUnsigned(size);

  switch (size) {
  case 8:
    MmsValue_setUint8(mms_unsigned, static_cast<uint8_t>(u));
    return mms_unsigned;
  case 16:
    MmsValue_setUint16(mms_unsigned, static_cast<uint16_t>(u));
    return mms_unsigned;
  case 32:
    MmsValue_setUint32(mms_unsigned, static_cast<uint32_t>(u));
    return mms_unsigned;
  default:
    throw RuntimeError{"invalid mms integer size"};
  }
}

MmsValue *GooseSignal::newMmsFloat(double d, int size) {
  switch (size) {
  case 32:
    return MmsValue_newFloat(static_cast<float>(d));
  case 64:
    return MmsValue_newDouble(d);
  default:
    throw RuntimeError{"invalid mms float size"};
  }
}

std::optional<GooseSignal::Type> GooseSignal::lookupMmsType(int mms_type) {
  auto check = [mms_type](Descriptor descriptor) {
    return descriptor.mms_type == mms_type;
  };

  auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
  if (descriptor != end(descriptors))
    return &*descriptor;
  else
    return std::nullopt;
}

std::optional<GooseSignal::Type>
GooseSignal::lookupMmsTypeName(char const *name) {
  auto check = [name](Descriptor descriptor) {
    return descriptor.name == name;
  };

  auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
  if (descriptor != end(descriptors))
    return &*descriptor;
  else
    return std::nullopt;
}

GooseSignal::GooseSignal(GooseSignal::Descriptor const *descriptor,
                         SignalData data, std::optional<Meta> meta)
    : signal_data(data), meta(meta.value_or(descriptor->default_meta)),
      descriptor(descriptor) {}

bool iec61850::operator==(GooseSignal &lhs, GooseSignal &rhs) {
  if (lhs.mmsType() != rhs.mmsType())
    return false;

  switch (lhs.mmsType()) {
  case MmsType::MMS_INTEGER:
  case MmsType::MMS_UNSIGNED:
  case MmsType::MMS_BIT_STRING:
  case MmsType::MMS_FLOAT:
    if (lhs.meta.size != rhs.meta.size)
      return false;
    break;
  default:
    break;
  }

  switch (lhs.signalType()) {
  case SignalType::BOOLEAN:
    return lhs.signal_data.b == rhs.signal_data.b;
  case SignalType::INTEGER:
    return lhs.signal_data.i == rhs.signal_data.i;
  case SignalType::FLOAT:
    return lhs.signal_data.f == rhs.signal_data.f;
  default:
    return false;
  }
}

bool iec61850::operator!=(GooseSignal &lhs, GooseSignal &rhs) {
  return !(lhs == rhs);
}

void GooseNode::onEvent(GooseSubscriber subscriber,
                        GooseNode::InputEventContext &ctx) noexcept {
  if (!GooseSubscriber_isValid(subscriber) ||
      GooseSubscriber_needsCommission(subscriber))
    return;

  int last_state_num = ctx.last_state_num;
  int state_num = GooseSubscriber_getStNum(subscriber);
  ctx.last_state_num = state_num;

  if (ctx.subscriber_config.trigger == InputTrigger::CHANGE &&
      !ctx.values.empty() && state_num == last_state_num)
    return;

  auto mms_values = GooseSubscriber_getDataSetValues(subscriber);

  if (MmsValue_getType(mms_values) != MmsType::MMS_ARRAY) {
    ctx.node->logger->warn("DataSet is not an array");
    return;
  }

  ctx.values.clear();

  unsigned int array_size = MmsValue_getArraySize(mms_values);
  for (unsigned int i = 0; i < array_size; i++) {
    auto mms_value = MmsValue_getElement(mms_values, i);
    auto goose_value = GooseSignal::fromMmsValue(mms_value);
    ctx.values.push_back(goose_value);
  }

  uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

  ctx.node->pushSample(timestamp);
}

void GooseNode::pushSample(uint64_t timestamp) noexcept {
  Sample *sample = sample_alloc(&input.pool);
  if (!sample) {
    logger->warn("Pool underrun");
    return;
  }

  sample->length = input.mappings.size();
  sample->flags = (int)SampleFlags::HAS_DATA;
  sample->signals = getInputSignals(false);

  if (input.with_timestamp) {
    sample->flags |= (int)SampleFlags::HAS_TS_ORIGIN;
    sample->ts.origin.tv_sec = timestamp / 1000;
    sample->ts.origin.tv_nsec = 1000 * (timestamp % 1000);
  }

  for (unsigned int signal = 0; signal < sample->length; signal++) {
    auto &mapping = input.mappings[signal];
    auto &values = input.contexts[mapping.subscriber].values;

    if (mapping.index >= values.size() || !values[mapping.index]) {
      auto signal_str = sample->signals->getByIndex(signal)->toString();
      logger->error("tried to access unavailable goose value for signal {}",
                    signal_str);
      continue;
    }

    if (mapping.type->mms_type != values[mapping.index]->mmsType()) {
      auto signal_str = sample->signals->getByIndex(signal)->toString();
      logger->error("received unexpected mms_type for signal {}", signal_str);
      continue;
    }

    sample->data[signal] = values[mapping.index]->signal_data;
  }

  if (queue_signalled_push(&input.queue, sample) != 1)
    logger->warn("Failed to enqueue samples");
}

void GooseNode::addSubscriber(GooseNode::InputEventContext &ctx) noexcept {
  GooseSubscriber subscriber;
  SubscriberConfig &sc = ctx.subscriber_config;

  ctx.node = this;

  subscriber = GooseSubscriber_create(sc.go_cb_ref.data(), NULL);

  if (sc.dst_address)
    GooseSubscriber_setDstMac(subscriber, sc.dst_address->data());

  if (sc.app_id)
    GooseSubscriber_setAppId(subscriber, *sc.app_id);

  GooseSubscriber_setListener(
      subscriber,
      [](GooseSubscriber goose_subscriber, void *event_context) {
        auto context = static_cast<InputEventContext *>(event_context);
        onEvent(goose_subscriber, *context);
      },
      &ctx);

  GooseReceiver_addSubscriber(input.receiver, subscriber);
}

void GooseNode::createReceiver() noexcept(false) {
  if (!input.interface_id.empty()) {
    input.receiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(input.receiver, input.interface_id.c_str());
  } else {
    RSessionError err;

    input.session = RSession_create();
    if (!input.session)
      throw RuntimeError{"failed to create R-GOOSE session"};

    err = RSession_setLocalAddress(input.session, input.local_address.c_str(),
                                   input.local_port);
    if (err != R_SESSION_ERROR_OK)
      throw RuntimeError("failed to set local address for R-GOOSE session");

    for (auto &key : keys) {
      err = RSession_addKey(input.session, key.id, key.data.data(),
                            key.data.size(), key.security, key.signature);
      if (err != R_SESSION_ERROR_OK)
        throw RuntimeError("failed to add key with id {} to R-GOOSE session",
                           key.id);
    }

    for (auto const &multicast_group : input.multicast_groups) {
      err = RSession_addMulticastGroup(input.session, multicast_group.c_str());
      if (err != R_SESSION_ERROR_OK)
        throw RuntimeError(
            "failed to add multicast group {} to R-GOOSE session",
            multicast_group);
    }

    input.receiver = GooseReceiver_createRemote(input.session);
  }

  for (auto &pair_key_context : input.contexts)
    addSubscriber(pair_key_context.second);
}

void GooseNode::destroyReceiver() noexcept {
  if (input.receiver) {
    stopReceiver();
    GooseReceiver_destroy(input.receiver);
    input.receiver = nullptr;
  }

  if (input.session) {
    RSession_destroy(input.session);
    input.session = nullptr;
  }
}

void GooseNode::startReceiver() noexcept(false) {
  GooseReceiver_start(input.receiver);

  if (!GooseReceiver_isRunning(input.receiver))
    throw RuntimeError{"iec61850-GOOSE receiver could not be started"};
}

void GooseNode::stopReceiver() noexcept {
  if (!GooseReceiver_isRunning(input.receiver))
    return;

  GooseReceiver_stop(input.receiver);
}

void GooseNode::createPublishers() {
  if (output.interface_id.empty()) {
    RSessionError err;

    output.session = RSession_create();
    if (!output.session)
      throw RuntimeError{"failed to create R-GOOSE session"};

    err = RSession_setLocalAddress(output.session, output.local_address.c_str(),
                                   output.local_port);
    if (err != R_SESSION_ERROR_OK)
      throw RuntimeError("failed to set local address {} for R-GOOSE session",
                         output.local_address);

    err = RSession_setRemoteAddress(
        output.session, output.remote_address.c_str(), output.remote_port);
    if (err != R_SESSION_ERROR_OK)
      throw RuntimeError("failed to set remote address {} for R-GOOSE session",
                         output.remote_address);

    for (auto &key : keys) {
      err = RSession_addKey(output.session, key.id, key.data.data(),
                            key.data.size(), key.security, key.signature);
      if (err != R_SESSION_ERROR_OK)
        throw RuntimeError("failed to add key with id {} to R-GOOSE session",
                           key.id);
    }

    RSession_setActiveKey(output.session, output.key_id);
  }

  for (auto &ctx : output.contexts) {
    if (!output.interface_id.empty()) {
      auto comm = CommParameters{/* vlanPriority */ 0,
                                 /* vlanId */ 0,
                                 ctx.config.app_id,
                                 {}};

      memcpy(comm.dstAddress, ctx.config.dst_address.data(),
             ctx.config.dst_address.size());

      ctx.publisher =
          GoosePublisher_createEx(&comm, output.interface_id.c_str(), false);
    } else {
      ctx.publisher =
          GoosePublisher_createRemote(output.session, ctx.config.app_id);
    }

    if (!ctx.config.go_id.empty())
      GoosePublisher_setGoID(ctx.publisher, ctx.config.go_id.data());

    GoosePublisher_setGoCbRef(ctx.publisher, ctx.config.go_cb_ref.data());
    GoosePublisher_setDataSetRef(ctx.publisher, ctx.config.data_set_ref.data());
    GoosePublisher_setConfRev(ctx.publisher, ctx.config.conf_rev);
    GoosePublisher_setTimeAllowedToLive(ctx.publisher,
                                        ctx.config.time_allowed_to_live);
  }
}

void GooseNode::destroyPublishers() noexcept {
  for (auto &ctx : output.contexts) {
    if (ctx.publisher) {
      GoosePublisher_destroy(ctx.publisher);
      ctx.publisher = nullptr;
    }
  }

  if (output.session) {
    RSession_destroy(output.session);
    output.session = nullptr;
  }
}

void GooseNode::startPublishers() noexcept(false) {
  output.resend_thread_stop = false;
  output.resend_thread = std::thread(resend_thread, &output);

  if (output.session) {
    RSessionError err = RSession_start(output.session);
    if (err != R_SESSION_ERROR_OK)
      throw RuntimeError("failed to start R-GOOSE publisher session");
  }
}

void GooseNode::stopPublishers() noexcept {
  if (output.resend_thread) {
    auto lock = std::unique_lock{output.send_mutex};
    output.resend_thread_stop = true;
    lock.unlock();

    output.resend_thread_cv.notify_all();
    output.resend_thread->join();
    output.resend_thread = std::nullopt;
  }
}

int GooseNode::_read(Sample *samples[], unsigned sample_count) {
  int available_samples;
  struct Sample *copies[sample_count];

  available_samples =
      queue_signalled_pull_many(&input.queue, (void **)copies, sample_count);
  sample_copy_many(samples, copies, available_samples);
  sample_decref_many(copies, available_samples);

  return available_samples;
}

void GooseNode::publish_values(GoosePublisher publisher,
                               std::vector<GooseSignal> &values, bool changed,
                               int burst) noexcept {
  auto data_set = LinkedList_create();

  for (auto &value : values) {
    LinkedList_add(data_set, value.toMmsValue());
  }

  if (changed)
    GoosePublisher_increaseStNum(publisher);

  do {
    GoosePublisher_publish(publisher, data_set);
  } while (changed && --burst > 0);

  LinkedList_destroyDeep(data_set,
                         (LinkedListValueDeleteFunction)MmsValue_delete);
}

void GooseNode::resend_thread(GooseNode::Output *output) noexcept {
  using namespace std::chrono;

  auto interval = duration_cast<steady_clock::duration>(
      duration<double>(output->resend_interval));
  auto lock = std::unique_lock{output->send_mutex};
  time_point<steady_clock> next_sample_time;

  // wait for the first GooseNode::_write call
  output->resend_thread_cv.wait(lock);

  while (!output->resend_thread_stop) {
    if (output->changed) {
      output->changed = false;
      next_sample_time = steady_clock::now() + interval;
    }

    auto status = output->resend_thread_cv.wait_until(lock, next_sample_time);

    if (status == std::cv_status::no_timeout || output->changed)
      continue;

    for (auto &ctx : output->contexts)
      publish_values(ctx.publisher, ctx.values, false);

    next_sample_time = next_sample_time + interval;
  }
}

int GooseNode::_write(Sample *samples[], unsigned sample_count) {
  auto lock = std::unique_lock{output.send_mutex};

  for (unsigned int i = 0; i < sample_count; i++) {
    auto sample = samples[i];

    for (auto &ctx : output.contexts) {
      bool changed = false;

      for (unsigned int data_index = 0; data_index < ctx.config.data.size();
           data_index++) {
        auto data = ctx.config.data[data_index];
        auto goose_value = data.default_value;
        auto signal = data.signal;
        if (signal && *signal < sample->length)
          goose_value.signal_data = sample->data[*signal];

        if (ctx.values.size() <= data_index) {
          changed = true;
          ctx.values.push_back(goose_value);
        } else if (ctx.values[data_index] != goose_value) {
          changed = true;
          ctx.values[data_index] = goose_value;
        }
      }

      if (changed) {
        output.changed = true;
        publish_values(ctx.publisher, ctx.values, changed, ctx.config.burst);
      }
    }
  }

  if (output.changed) {
    lock.unlock();
    output.resend_thread_cv.notify_all();
  }

  return sample_count;
}

GooseNode::GooseNode(const uuid_t &id, const std::string &name)
    : Node(id, name) {
  input.contexts = {};
  input.mappings = {};
  input.queue_length = 1024;
  output.changed = false;
  output.resend_interval = 1.;
  output.resend_thread = std::nullopt;
}

GooseNode::~GooseNode() {
  int err __attribute__((unused));

  destroyReceiver();
  destroyPublishers();

  err = queue_signalled_destroy(&input.queue);

  err = pool_destroy(&input.pool);
}

int GooseNode::parse(json_t *json) {
  int ret;
  json_error_t err;

  ret = Node::parse(json);
  if (ret)
    return ret;

  json_t *json_keys = nullptr;
  json_t *json_in = nullptr;
  json_t *json_out = nullptr;
  ret = json_unpack_ex(json, &err, 0,          //
                       "{ s:?o, s:?o, s:?o }", //
                       "keys", &json_keys,     //
                       "in", &json_in,         //
                       "out", &json_out);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  if (json_keys) {
    size_t index;
    json_t *json_key;
    assert(json_is_array(json_keys));
    keys.reserve(json_array_size(json_keys));
    json_array_foreach(json_keys, index, json_key) {
      assert(json_is_object(json_key));
      parseSessionKey(json_key);
    }
  }

  if (json_in)
    parseInput(json_in);

  if (json_out)
    parseOutput(json_out);

  return 0;
}

void GooseNode::parseInput(json_t *json) {
  int ret;
  json_error_t err;

  json_t *json_subscribers = nullptr;
  json_t *json_signals = nullptr;
  int routed = false;
  char const *local_address = "localhost";
  int local_port = 102;
  json_t *json_multicast_groups = nullptr;
  char const *interface_id = "lo";
  int with_timestamp = true;
  ret = json_unpack_ex(json, &err, 0,                                      //
                       "{ s:o, s:o, s:?b, s:?s, s:?i, s:?o, s:?s, s:?b }", //
                       "subscribers", &json_subscribers,                   //
                       "signals", &json_signals,                           //
                       "routed", &routed,                                  //
                       "local_address", &local_address,                    //
                       "local_port", &local_port,                          //
                       "multicast_groups", &json_multicast_groups,         //
                       "interface", &interface_id,                         //
                       "with_timestamp", &with_timestamp);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  if (routed) {
    input.local_address = local_address;
    input.local_port = local_port;

    if (json_multicast_groups) {
      size_t index;
      json_t *json_multicast_group;
      assert(json_is_array(json_multicast_groups));
      input.multicast_groups.reserve(json_array_size(json_multicast_groups));
      json_array_foreach(json_multicast_groups, index, json_multicast_group) {
        assert(json_is_string(json_multicast_group));
        input.multicast_groups.emplace_back(
            json_string_value(json_multicast_group));
      }
    }

    if (keys.empty())
      throw RuntimeError{"missing session keys for R-GOOSE"};
  } else {
    input.interface_id = interface_id;
  }

  parseSubscribers(json_subscribers, input.contexts);
  parseInputSignals(json_signals, input.mappings);

  input.with_timestamp = with_timestamp;
}

void GooseNode::parseSessionKey(json_t *json) {
  int ret;
  json_error_t err;

  int id;
  char const *security_str;
  char const *signature_str;
  char const *data_str = nullptr;
  char const *data_base64 = nullptr;
  ret = json_unpack_ex(json, &err, 0,                   //
                       "{ s:i, s:s, s:s, s:?s, s:?s }", //
                       "id", &id,                       //
                       "security", &security_str,       //
                       "signature", &signature_str,     //
                       "string", &data_str,             //
                       "base64", &data_base64);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  RSecurityAlgorithm security;
  if (security_str == "aes_128_gcm"sv)
    security = R_SESSION_SEC_ALGO_AES_128_GCM;
  else if (security_str == "aes_256_gcm"sv)
    security = R_SESSION_SEC_ALGO_AES_256_GCM;
  else if (signature_str == "none"sv)
    security = R_SESSION_SEC_ALGO_NONE;
  else
    throw RuntimeError("unknown security algorithm {}", security_str);

  RSignatureAlgorithm signature;
  if (signature_str == "aes_gmac_64"sv)
    signature = R_SESSION_SIG_ALGO_AES_GMAC_64;
  else if (signature_str == "aes_gmac_128"sv)
    signature = R_SESSION_SIG_ALGO_AES_GMAC_128;
  else if (signature_str == "hmac_sha256_80"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA256_80;
  else if (signature_str == "hmac_sha256_128"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA256_128;
  else if (signature_str == "hmac_sha256_256"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA256_256;
  else if (signature_str == "hmac_sha3_80"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA3_80;
  else if (signature_str == "hmac_sha3_128"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA3_128;
  else if (signature_str == "hmac_sha3_256"sv)
    signature = R_SESSION_SIG_ALGO_HMAC_SHA3_256;
  else if (signature_str == "none"sv)
    signature = R_SESSION_SIG_ALGO_NONE;
  else
    throw RuntimeError("unknown signature algorithm {}", signature_str);

  std::vector<uint8_t> data;
  if (data_str && data_base64)
    throw RuntimeError(
        "can't use both 'base64' and 'string' for R-GOOSE key with id {}", id);
  else if (data_str) {
    auto data_sv = std::string_view(data_str);
    data = std::vector<uint8_t>(begin(data_sv), end(data_sv));
  } else if (data_base64) {
    data = base64::decode(data_base64);
  } else {
    throw RuntimeError(
        "missing one of 'base64' or 'string' for R-GOOSE key with id {}", id);
  }

  keys.emplace_back(SessionKey{
      .id = id,
      .security = security,
      .signature = signature,
      .data = data,
  });
}

void GooseNode::parseSubscriber(json_t *json, GooseNode::SubscriberConfig &sc) {
  int ret;
  json_error_t err;

  char *go_cb_ref = nullptr;
  char *dst_address_str = nullptr;
  char *trigger = nullptr;
  int app_id = 0;
  ret = json_unpack_ex(json, &err, 0, "{ s:s, s:?s, s:?s, s:?i }", //
                       "go_cb_ref", &go_cb_ref,                    //
                       "trigger", &trigger,                        //
                       "dst_address", &dst_address_str,            //
                       "app_id", &app_id);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  sc.go_cb_ref = std::string{go_cb_ref};

  if (!trigger || !strcmp(trigger, "always"))
    sc.trigger = InputTrigger::ALWAYS;
  else if (!strcmp(trigger, "change"))
    sc.trigger = InputTrigger::CHANGE;
  else
    throw RuntimeError("Unknown trigger type");

  if (dst_address_str) {
    std::optional dst_address = stringToMac(dst_address_str);
    if (!dst_address)
      throw RuntimeError("Invalid dst_address");
    sc.dst_address = *dst_address;
  }

  if (app_id != 0)
    sc.app_id = static_cast<int16_t>(app_id);
}

void GooseNode::parseSubscribers(
    json_t *json, std::map<std::string, InputEventContext> &ctx) {
  char const *key;
  json_t *json_subscriber;

  if (!json_is_object(json))
    throw RuntimeError("subscribers is not an object");

  json_object_foreach(json, key, json_subscriber) {
    SubscriberConfig sc;

    parseSubscriber(json_subscriber, sc);

    ctx[key] = InputEventContext{sc};
  }
}

void GooseNode::parseInputSignals(
    json_t *json, std::vector<GooseNode::InputMapping> &mappings) {
  int ret;
  json_error_t err;
  int index;
  json_t *value;

  mappings.clear();

  json_array_foreach(json, index, value) {
    char *mapping_subscriber;
    unsigned int mapping_index;
    char *mapping_type_name;
    ret = json_unpack_ex(value, &err, 0,                    //
                         "{ s: s, s: i, s: s }",            //
                         "subscriber", &mapping_subscriber, //
                         "index", &mapping_index,           //
                         "mms_type", &mapping_type_name);
    if (ret)
      throw ConfigError(json, err, "node-config-node-iec61850-8-1");

    auto mapping_type =
        GooseSignal::lookupMmsTypeName(mapping_type_name).value();

    auto const &config_type = in.signals->getByIndex(index)->type;
    if (mapping_type->signal_type != config_type)
      throw RuntimeError("configured 'type' {} for signal {} "
                         "does not match the 'mms_type' {}",
                         signalTypeToString(config_type), index,
                         mapping_type->name);

    mappings.push_back(InputMapping{
        mapping_subscriber,
        mapping_index,
        mapping_type,
    });
  }
}

void GooseNode::parseOutput(json_t *json) {
  int ret;
  json_error_t err;

  json_t *json_publishers = nullptr;
  int routed = false;
  char const *local_address = "localhost";
  int local_port = 0;
  char const *remote_address = "localhost";
  int remote_port = 102;
  int key_id = -1;
  char const *interface_id = "lo";
  ret = json_unpack_ex(
      json, &err, 0,
      "{ s:o, s:?b, s:?s, s:?i, s:?s, s:?i, s:?i, s:?s, s:?f }", //
      "publishers", &json_publishers,                            //
      "routed", &routed,                                         //
      "local_address", &local_address,                           //
      "local_port", &local_port,                                 //
      "remote_address", &remote_address,                         //
      "remote_port", &remote_port,                               //
      "key_id", &key_id,                                         //
      "interface", &interface_id,                                //
      "resend_interval", &output.resend_interval);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  if (!routed) {
    output.interface_id = interface_id;
  } else {
    output.local_address = local_address;
    output.local_port = local_port;
    output.remote_address = remote_address;
    output.remote_port = remote_port;

    if (keys.empty())
      throw RuntimeError{"missing session 'keys' for R-GOOSE"};
    else if (keys.size() == 1 && key_id == -1)
      key_id = keys[0].id;
    else if (key_id == -1)
      throw RuntimeError{"missing 'key_id' for R-GOOSE out"};
    else if (end(keys) !=
             std::find_if(begin(keys), end(keys), [key_id](auto const &key) {
               return key.id == key_id;
             }))
      throw RuntimeError{"'key_id' does not correspond to any known key"};

    output.key_id = key_id;
  }

  parsePublishers(json_publishers, output.contexts);
}

void GooseNode::parsePublisherData(json_t *json,
                                   std::vector<OutputData> &data) {
  int ret;
  json_error_t err;
  int index;
  json_t *json_signal_or_value;

  if (!json_is_array(json))
    throw RuntimeError("publisher data is not an array");

  json_array_foreach(json, index, json_signal_or_value) {
    char const *mms_type = nullptr;
    char const *signal_str = nullptr;
    json_t *json_value = nullptr;
    int bitstring_size = -1;
    ret = json_unpack_ex(json_signal_or_value, &err, 0,
                         "{ s: s, s?: s, s?: o, s?: i }", //
                         "mms_type", &mms_type,           //
                         "signal", &signal_str,           //
                         "value", &json_value,            //
                         "mms_bitstring_size", &bitstring_size);
    if (ret)
      throw ConfigError(json, err, "node-config-node-iec61850-8-1");

    auto goose_type = GooseSignal::lookupMmsTypeName(mms_type).value();
    std::optional<GooseSignal::Meta> meta = std::nullopt;

    if (goose_type->mms_type == MmsType::MMS_BIT_STRING && bitstring_size != -1)
      meta = {.size = bitstring_size};

    auto signal_data = SignalData{};

    if (json_value) {
      ret = signal_data.parseJson(goose_type->signal_type, json_value);
      if (ret)
        throw ConfigError(json, err, "node-config-node-iec61850-8-1");
    }

    auto signal = std::optional<int>{};

    if (signal_str)
      signal = out.signals->getIndexByName(signal_str);

    OutputData value = {.signal = signal,
                        .default_value =
                            GooseSignal{goose_type, signal_data, meta}};

    data.push_back(value);
  };
}

void GooseNode::parsePublisher(json_t *json, PublisherConfig &pc) {
  int ret;
  json_error_t err;

  char *go_id = nullptr;
  char *go_cb_ref = nullptr;
  char *data_set_ref = nullptr;
  char *dst_address_str = nullptr;
  int app_id = 0;
  int conf_rev = 0;
  int time_allowed_to_live = 0;
  int burst = 1;
  json_t *json_data = nullptr;
  ret = json_unpack_ex(json, &err, 0,
                       "{ s:?s, s:s, s:s, s:?s, s:i, s:i, s:i, s?:i, s:o }", //
                       "go_id", &go_id,                                      //
                       "go_cb_ref", &go_cb_ref,                              //
                       "data_set_ref", &data_set_ref,                        //
                       "dst_address", &dst_address_str,                      //
                       "app_id", &app_id,                                    //
                       "conf_rev", &conf_rev,                                //
                       "time_allowed_to_live", &time_allowed_to_live,        //
                       "burst", &burst,                                      //
                       "data", &json_data);
  if (ret)
    throw ConfigError(json, err, "node-config-node-iec61850-8-1");

  if (!output.interface_id.empty()) {
    std::optional dst_address = stringToMac(dst_address_str);
    if (!dst_address)
      throw RuntimeError("Invalid dst_address");

    pc.dst_address = *dst_address;
  }

  if (go_id)
    pc.go_id = go_id;

  pc.go_cb_ref = go_cb_ref;
  pc.data_set_ref = data_set_ref;
  pc.app_id = app_id;
  pc.conf_rev = conf_rev;
  pc.time_allowed_to_live = time_allowed_to_live;
  pc.burst = burst;

  parsePublisherData(json_data, pc.data);
}

void GooseNode::parsePublishers(json_t *json, std::vector<OutputContext> &ctx) {
  int index;
  json_t *json_publisher;

  json_array_foreach(json, index, json_publisher) {
    PublisherConfig pc;

    parsePublisher(json_publisher, pc);

    ctx.push_back(OutputContext{pc});
  }
}

std::vector<int> GooseNode::getPollFDs() {
  return {queue_signalled_fd(&input.queue)};
}

int GooseNode::prepare() {
  int ret;

  ret = pool_init(&input.pool, input.queue_length,
                  SAMPLE_LENGTH(getInputSignals(false)->size()));
  if (ret)
    return ret;

  ret = queue_signalled_init(&input.queue, input.queue_length);
  if (ret)
    return ret;

  if (in.enabled) {
    createReceiver();
    createPublishers();
  }

  return Node::prepare();
}

int GooseNode::start() {
  if (in.enabled)
    startReceiver();

  if (out.enabled)
    startPublishers();

  return Node::start();
}

int GooseNode::stop() {
  int err __attribute__((unused));

  stopReceiver();
  stopPublishers();

  err = queue_signalled_close(&input.queue);

  return Node::stop();
}

// Register node
static char name[] = "iec61850-8-1";
static char description[] = "IEC 61850-8-1 (GOOSE)";
static NodePlugin<GooseNode, name, description,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL,
                  1>
    p;
