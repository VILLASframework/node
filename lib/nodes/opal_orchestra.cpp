/* Node type: OPAL-RT Orchestra co-simulation client.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>

#include <fmt/chrono.h>
#include <fmt/std.h>
#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>

extern "C" {
#include <RTAPI.h>
}

#include <villas/config_helper.hpp>
#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/opal_orchestra/ddf.hpp>
#include <villas/nodes/opal_orchestra/error.hpp>
#include <villas/nodes/opal_orchestra/locks.hpp>
#include <villas/nodes/opal_orchestra/signal.hpp>
#include <villas/sample.hpp>
#include <villas/signal_type.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/fs.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::orchestra;

// An OpalOrchestraMapping maps one or more VILLASnode signals to an Orchestra data item.
class OpalOrchestraMapping {
public:
  std::shared_ptr<DataItem> item;
  std::string path;
  std::vector<Signal::Ptr> signals;

  // Cached signal indices
  // We keep a vector of indices to map the signal index in the signal list.
  SignalList::Ptr signalList; // Signal list for which the indices are valid.
  std::vector<std::optional<unsigned>> indices;

  // Run-time members which will be retrieved from Orchestra in prepare().
  unsigned short key;
  char *buffer;
  unsigned int typeSize; // sizeof() of the signal type. See RTSignalType.
  unsigned short length;

  OpalOrchestraMapping(std::shared_ptr<DataItem> item, std::string path)
      : item(item), path(std::move(path)), signals(), signalList(), indices(),
        key(0), buffer(nullptr), typeSize(0), length(0) {}

  void addSignal(Signal::Ptr signal, std::optional<unsigned> orchestraIdx) {
    if (!orchestraIdx) {
      orchestraIdx = signals.size();
    }

    if (*orchestraIdx < signals.size()) {
      if (signals[*orchestraIdx]) {
        throw RuntimeError("Index {} of Orchestra signal already mapped",
                           *orchestraIdx);
      }
    } else {
      signals.resize(*orchestraIdx + 1, nullptr);
      item->length = signals.size();
    }

    signals[*orchestraIdx] = signal;
  }

  void check() {
    if (signals.empty()) {
      throw RuntimeError("No signal mapped to Orchestra signal '{}'",
                         item->name);
    }

    if (item->name.empty()) {
      throw RuntimeError("Signal name cannot be empty");
    }

    if (item->name.find_first_of(" \t\n") != std::string::npos) {
      throw RuntimeError("Signal name '{}' contains whitespace", item->name);
    }

    if (item->name.find_first_of(",#") != std::string::npos) {
      throw RuntimeError("Signal name '{}' contains comma or hash-tag",
                         item->name);
    }

    if (item->name.size() > DataItem::IDENTIFIER_NAME_LENGTH - 1) {
      throw RuntimeError("Signal name '{}' is too long", item->name);
    }

    auto firstSignal = signals[0];

    for (auto &signal : signals) {
      if (signal->type != firstSignal->type) {
        throw RuntimeError("Signal type mismatch: {} vs {} for signal '{}'",
                           signalTypeToString(signal->type),
                           signalTypeToString(firstSignal->type), signal->name);
      }

      if (signal->init.f != firstSignal->init.f) {
        throw RuntimeError("Signal default value mismatch: {} vs {} for signal "
                           "'{}'",
                           signal->init.toString(signal->type),
                           firstSignal->init.toString(firstSignal->type),
                           signal->name);
      }
    }

    auto orchestraType = toOrchestraSignalType(firstSignal->type);
    if (item->type != orchestraType) {
      throw RuntimeError("Signal type mismatch: {} vs {} for signal '{}'",
                         signalTypeToString(item->type),
                         signalTypeToString(orchestraType), item->name);
    }
  }

  void prepare(unsigned int connectionKey) {
    auto ret = RTGetInfoForItem(path.c_str(), &typeSize, &length);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get info for signal '{}'", item->name);
    }

    ret = RTGetKeyForItem(path.c_str(), &key);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get key for signal '{}'", item->name);
    }

    ret = RTGetBuffer(connectionKey, key, (void **)&buffer);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get buffer for signal '{}'", item->name);
    }

    auto logger = Log::get("orchestra");
    logger->trace(
        "Prepared mapping: path='{}', type={}, typeSize={}, key={}, buffer={}"
        "length={}, default={}",
        path, orchestra::signalTypeToString(item->type), key, buffer, typeSize,
        length, item->defaultValue);
  }

  void publish(struct Sample *smp) {
    updateIndices(smp->signals);

    auto *orchestraDataPtr = buffer;
    for (auto &index : indices) {
      if (!index || *index >= smp->length) {
        orchestraDataPtr += typeSize;
        continue; // Unused index or index out of range.
      }

      auto signal = smp->signals->getByIndex(*index);
      if (!signal) {
        throw RuntimeError("Signal {} not found", index);
      }

      toOrchestraSignalData(orchestraDataPtr, item->type, smp->data[*index],
                            signal->type);

      orchestraDataPtr += typeSize;
    }
  }

  void subscribe(struct Sample *smp) {
    updateIndices(smp->signals);

    auto *orchestraDataPtr = buffer;
    for (auto &index : indices) {
      if (!index || *index >= smp->capacity) {
        continue; // Unused index or index out of range.
      }

      for (unsigned i = smp->length; i < *index; i++) {
        smp->data[i].i = 0;
      }

      auto signal = smp->signals->getByIndex(*index);
      if (!signal) {
        throw RuntimeError("Signal {} not found", *index);
      }

      node::SignalType villasType;
      SignalData villasData =
          toNodeSignalData(orchestraDataPtr, item->type, villasType);

      smp->data[*index] = villasData.cast(villasType, signal->type);

      if (index >= static_cast<int>(smp->length)) {
        smp->length = *index + 1;
      }

      orchestraDataPtr += typeSize;
    }
  }

protected:
  void updateIndices(SignalList::Ptr newSignalList) {
    if (signalList == newSignalList) {
      return; // Already up to date.
    }

    indices.clear();

    for (const auto &signal : signals) {
      if (signal) {
        auto idx = newSignalList->getIndexByName(signal->name);
        if (idx < 0) {
          throw RuntimeError("Signal '{}' not found", signal->name);
        }

        indices.push_back(idx);
      } else {
        indices.emplace_back(); // Unused index
      }
    }

    signalList = newSignalList;
  }
};

class OpalOrchestraNode : public Node {

protected:
  Task task; // The task which is used to pace the node in asynchronous mode.

  unsigned int
      connectionKey; // A connection key identifies a connection between a specific combo of  Orchestra's framework and client.
  unsigned int *status;

  Domain domain; // The domain to which the node belongs.

  std::unordered_map<std::shared_ptr<DataItem>, OpalOrchestraMapping>
      subscribeMappings;
  std::unordered_map<std::shared_ptr<DataItem>, OpalOrchestraMapping>
      publishMappings;

  double rate;
  std::optional<fs::path> dataDefinitionFilename;

  std::chrono::seconds connectTimeout;
  std::optional<std::chrono::microseconds>
      flagDelay; // Define a delay to wait, this will call the system function usleep and free the CPU.
  std::optional<std::chrono::microseconds>
      flagDelayTool; // Force the local Orchestra communication to be made with flag instead of semaphore when using an external communication process.
  bool skipWaitToGo; // Skip wait-to-go step during start.
  bool dataDefinitionFileOverwrite; // Overwrite the data definition file (DDF).
  bool
      dataDefinitionFileWriteOnly; // Overwrite the data definition file (DDF) and terminate VILLASnode.

  int _read(struct Sample *smps[], unsigned cnt) override {
    if (dataDefinitionFileWriteOnly) {
      logger->warn("Stopping node after writing the DDF file");
      setState(State::STOPPING);
      return -1;
    }

    assert(cnt == 1);

    if (!domain.synchronous) {
      task.wait();
    }

    try {
      RTSubscribeLockGuard guard(connectionKey);

      auto *smp = smps[0];
      smp->signals = getInputSignals(false);
      smp->length = 0;
      smp->flags = 0;

      for (auto &[item, mapping] : publishMappings) {
        mapping.subscribe(smp);
      }

      if (smp->length > 0) {
        smp->flags |= (int)SampleFlags::HAS_DATA;
      }

      return 1;
    } catch (const RTError &e) {
      if (e.returnCode == RTAPI_SUBSCRIBE_FAILED_RTLAB_SUBSYSTEM_UNMAPPED) {
        logger->warn("Orchestra Framework has been stopped / unmapped");
        setState(State::STOPPING);
      } else {
        logger->error("Failed to read from Orchestra: {}", e.what());
      }
      return -1;
    }
  }

  int _write(struct Sample *smps[], unsigned cnt) override {
    if (dataDefinitionFileWriteOnly) {
      logger->warn("Stopping node after writing the DDF file");
      setState(State::STOPPING);
      return -1;
    }

    assert(cnt == 1);

    try {
      RTPublishLockGuard guard(connectionKey);

      auto *smp = smps[0];

      for (auto &[item, mapping] : subscribeMappings) {
        mapping.publish(smp);
      }

      return 1;
    } catch (const RTError &e) {
      if (e.returnCode == RTAPI_PUBLISH_FAILED_RTLAB_SUBSYSTEM_UNMAPPED) {
        logger->warn("Orchestra Framework has been stopped / unmapped");
        setState(State::STOPPING);
      } else {
        logger->error("Failed to write to Orchestra: {}", e.what());
      }
      return -1;
    }
  }

public:
  OpalOrchestraNode(const uuid_t &id = {}, const std::string &name = "",
                    unsigned int key = 0)
      : Node(id, name), task(), connectionKey(key), status(nullptr), domain(),
        subscribeMappings(), publishMappings(), rate(1), connectTimeout(5),
        skipWaitToGo(false), dataDefinitionFileOverwrite(false),
        dataDefinitionFileWriteOnly(false) {}

  void parseSignals(json_t *json, SignalList::Ptr signals, DataSet &dataSet,
                    std::unordered_map<std::shared_ptr<DataItem>,
                                       OpalOrchestraMapping> &mappings) {
    if (!json_is_array(json)) {
      throw ConfigError(json, "node-config-node-opal-orchestra-signals",
                        "Signals must be an array");
    }

    size_t i;
    json_t *json_signal;
    json_error_t err;

    json_array_foreach(json, i, json_signal) {
      auto signal = signals->getByIndex(i);

      const char *nme = nullptr;
      const char *typ = nullptr;
      int oi = -1;

      auto ret = json_unpack_ex(json_signal, &err, 0, "{ s?: s, s?: s, s?: i }",
                                "orchestra_name", &nme, "orchestra_type", &typ,
                                "orchestra_index", &oi);
      if (ret) {
        throw ConfigError(json_signal, err,
                          "node-config-node-opal-orchestra-signals");
      }

      std::optional<unsigned> orchestraIdx;

      if (oi >= 0) {
        orchestraIdx = oi;
      }

      auto defaultValue =
          signal->init.cast(signal->type, node::SignalType::FLOAT);

      auto orchestraType = typ ? orchestra::signalTypeFromString(typ)
                               : orchestra::toOrchestraSignalType(signal->type);

      auto orchestraName = nme ? nme : signal->name;

      bool inserted = false;
      auto item = dataSet.upsertItem(orchestraName, inserted);

      if (inserted) {
        item->type = orchestraType;
        item->defaultValue = defaultValue.f;

        mappings.emplace(item, OpalOrchestraMapping(item, orchestraName));
      }

      auto &mapping = mappings.at(item);
      mapping.addSignal(signal, orchestraIdx);
    }
  }

  int parse(json_t *json) override {
    int ret = Node::parse(json);
    if (ret)
      return ret;

    const char *dn = nullptr;
    const char *ddf = nullptr;
    json_t *json_in_signals = nullptr;
    json_t *json_out_signals = nullptr;
    json_t *json_connection = nullptr;
    json_t *json_connect_timeout = nullptr;
    json_t *json_flag_delay = nullptr;
    json_t *json_flag_delay_tool = nullptr;

    int sw = -1;
    int ow = -1;
    int owo = -1;
    int sy = -1;
    int sts = -1;

    json_error_t err;
    ret = json_unpack_ex(
        json, &err, 0,
        "{ s: s, s?: b, s?: b, s?: o, s?: s, s?: o, s?: o, s?: o, s?: b, s?: "
        "b, s?: b, s?: F, s?: { s?: o }, s?: { s?: o } }",
        "domain", &dn, "synchronous", &sy, "states", &sts, "connection",
        &json_connection, "ddf", &ddf, "connect_timeout", &json_connect_timeout,
        "flag_delay", &json_flag_delay, "flag_delay_tool",
        &json_flag_delay_tool, "skip_wait_to_go", &sw, "ddf_overwrite", &ow,
        "ddf_overwrite_only", &owo, "rate", &rate, "in", "signals",
        &json_in_signals, "out", "signals", &json_out_signals);
    if (ret) {
      throw ConfigError(json, err, "node-config-node-opal-orchestra");
    }

    domain.name = dn;

    if (ddf) {
      dataDefinitionFilename = ddf;
    }

    if (sy >= 0) {
      domain.synchronous = sy > 0;
    }

    if (sts >= 0) {
      domain.states = sts > 0;
    }

    if (sw >= 0) {
      skipWaitToGo = sw > 0;
    }

    if (ow >= 0) {
      dataDefinitionFileOverwrite = ow > 0;
    }

    if (owo >= 0) {
      dataDefinitionFileWriteOnly = owo > 0;
    }

    if (json_connect_timeout) {
      connectTimeout =
          parse_duration<std::chrono::seconds>(json_connect_timeout);
    }

    if (json_flag_delay) {
      *flagDelay = parse_duration<std::chrono::microseconds>(json_flag_delay);
    }

    if (json_flag_delay_tool) {
      *flagDelayTool =
          parse_duration<std::chrono::microseconds>(json_flag_delay_tool);
    }

    if (json_connection) {
      domain.connection = Connection::fromJson(json_connection);
    }

    if (json_in_signals) {
      parseSignals(json_in_signals, in.getSignals(false), domain.publish,
                   publishMappings);
    }

    if (json_out_signals) {
      parseSignals(json_out_signals, out.getSignals(false), domain.subscribe,
                   subscribeMappings);
    }

    return 0;
  }

  void checkDuplicateNames(const DataSet &a, const DataSet &b) {
    for (auto &[name, item] : a.items) {
      if (b.items.find(name) != b.items.end()) {
        throw RuntimeError(
            "Orchestra signal '{}' is used for both publish and subscribe",
            name);
      }
    }
  }

  int check() override {
    if (dataDefinitionFilename) {
      if (!fs::exists(*dataDefinitionFilename) &&
          !dataDefinitionFileOverwrite) {
        throw RuntimeError("OPAL-RT Orchestra Data Definition file (DDF) at "
                           "'{}' does not exist",
                           *dataDefinitionFilename);
      }
    } else {
      if (dataDefinitionFileOverwrite) {
        throw RuntimeError(
            "The option 'ddf_overwrite' requires the option 'ddf' to be set");
      }
    }

    checkDuplicateNames(domain.publish, domain.subscribe);

    return Node::check();
  }

  int prepare() override {
    // Write DDF.
    if (dataDefinitionFilename &&
        (dataDefinitionFileOverwrite || dataDefinitionFileWriteOnly)) {

      // TODO: Possibly merge Orchestra domains from all nodes into one DDF.
      auto ddf = DataDefinitionFile();
      ddf.domains.push_back(domain);
      ddf.writeToFile(*dataDefinitionFilename);

      logger->info("Wrote Orchestra Data Definition file (DDF) to '{}'",
                   *dataDefinitionFilename);

      if (dataDefinitionFileWriteOnly) {
        return Node::prepare();
      }
    }

    logger->debug("Connecting to Orchestra framework: domain={}, ddf={}, "
                  "connection_key={}",
                  domain.name,
                  dataDefinitionFilename ? *dataDefinitionFilename : "none",
                  connectionKey);

    RTConnectionLockGuard guard(connectionKey);

    auto ret =
        dataDefinitionFilename
            ? RTConnectWithFile(dataDefinitionFilename->c_str(),
                                domain.name.c_str(), connectTimeout.count())
            : RTConnect(domain.name.c_str(), connectTimeout.count());
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to connect to Orchestra framework");
    }

    logger->info("Connected to Orchestra framework: domain={}", domain.name);

    ret = RTGetConnectionStatusPtr(connectionKey, &status);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get connection status pointer");
    }

    if (flagDelay) {
      ret = RTDefineFlagDelay(flagDelay->count());
      if (ret != RTAPI_SUCCESS) {
        throw RTError(ret, "Failed to set flag delay");
      }
    }

    if (flagDelayTool) {
      ret = RTUseFlagWithTool(flagDelayTool->count());
      if (ret != RTAPI_SUCCESS) {
        throw RTError(ret, "Failed to set flag with tool");
      }
    }

    ret = RTSetSkipWaitToGoAtConnection(skipWaitToGo);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to check ready to go");
    }

    if (std::shared_ptr<ConnectionRemote> c =
            std::dynamic_pointer_cast<ConnectionRemote>(domain.connection)) {
      ret = RTSetupRefMemRemoteConnection(c->name.c_str(), c->pciIndex);
      if (ret != RTAPI_SUCCESS) {
        throw RTError(ret,
                      "Failed to setup reflective memory remote connection");
      }
    }

    for (auto &[item, mapping] : publishMappings) {
      mapping.prepare(connectionKey);
    }

    for (auto &[item, mapping] : subscribeMappings) {
      mapping.prepare(connectionKey);
    }

    char *bufferPublish, *bufferSubscribe;
    unsigned int bufferPublishSize, bufferSubscribeSize;
    ret = RTGetBufferInfo(connectionKey, &bufferPublish, &bufferPublishSize,
                          &bufferSubscribe, &bufferSubscribeSize);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get buffer info");
    }
    logger->debug("Buffer publish:   size={}, ptr={:p}", bufferPublishSize,
                  bufferPublish);
    logger->debug("Buffer subscribe: size={}, ptr={:p}", bufferSubscribeSize,
                  bufferSubscribe);

    return Node::prepare();
  }

  int start() override {
    if (dataDefinitionFileWriteOnly) {
      return Node::start();
    }

    RTConnectionLockGuard guard(connectionKey);

    RTWaitReadyToGo();

    if (!domain.synchronous) {
      task.setRate(rate);
    }

    return Node::start();
  }

  int stop() override {
    int ret = Node::stop();
    if (ret)
      return ret;

    RTConnectionLockGuard guard(connectionKey);

    if (!domain.synchronous) {
      task.stop();
    }

    auto ret2 = RTDisconnect();
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret2, "Failed to disconnect");
    }

    return 0;
  }

  const std::string &getDetails() override {
    details = fmt::format("domain={}", domain.name);

    if (dataDefinitionFilename) {
      details += fmt::format(", ddf={}", *dataDefinitionFilename);
    }

    details += fmt::format(", connect_timeout={}, skip_wait_to_go={}",
                           connectTimeout, skipWaitToGo);

    if (flagDelay)
      details += fmt::format(", flag_delay = {}", *flagDelay);

    if (flagDelayTool)
      details += fmt::format(", flag_delay_tool = {}", *flagDelayTool);

    details += fmt::format(
        ", #publish_items={}, #subcribe_items={}, "
        "connection_key={}, synchronous={}, states={}, "
        "multiple_publish_allowed={}, connection={{ {} }}",
        domain.publish.items.size(), domain.subscribe.items.size(),
        connectionKey, domain.synchronous, domain.states,
        domain.multiplePublishAllowed, domain.connection->getDetails());

    return details;
  }

  std::vector<int> getPollFDs() override {
    if (!domain.synchronous) {
      return {task.getFD()};
    }

    return {};
  }
};

class OpalOrchestraNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  Node *make(const uuid_t &id = {}, const std::string &nme = "") override {
    RTConnectionLockGuard guard(0);

    unsigned int connectionKey;
    auto ret = RTNewConnectionKey(&connectionKey);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to create new Orchestra node");
    }

    auto *n = new OpalOrchestraNode(id, nme, connectionKey);

    init(n);

    return n;
  }

  int getFlags() const override {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL |
           (int)NodeFactory::Flags::HIDDEN;
  }

  std::string getName() const override { return "opal.orchestra"; }

  std::string getDescription() const override {
    return "OPAL-RT Orchestra client";
  }
};

static OpalOrchestraNodeFactory p;
