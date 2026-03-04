/* Node type: OPAL-RT Orchestra co-simulation client.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>

#include <fmt/chrono.h>
#include <fmt/std.h>
#include <jansson.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>

extern "C" {
#include <RTAPI.h>

__attribute__((weak)) void op_license_feature_register() {}
}

#include <villas/config_helper.hpp>
#include <villas/exceptions.hpp>
#include <villas/fs.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/opal_orchestra/ddf.hpp>
#include <villas/nodes/opal_orchestra/error.hpp>
#include <villas/nodes/opal_orchestra/locks.hpp>
#include <villas/nodes/opal_orchestra/signal.hpp>
#include <villas/sample.hpp>
#include <villas/signal_type.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::orchestra;

// An OpalOrchestraMapping maps one or more VILLASnode signals to an Orchestra data item.
class OpalOrchestraMapping {
public:
  std::shared_ptr<DataItem> item;
  std::string path;
  Signal::Ptr firstSignal;
  Signal::Ptr lastSignal;

  unsigned villasOffset;
  unsigned villasLength;

  // Run-time members which will be retrieved from Orchestra in prepare().
  unsigned short orchestraLength;
  unsigned short orchestraKey;
  char *orchestraBuffer;
  unsigned int orchestraTypeSize; // sizeof() of the signal type. See RTSignalType.

  OpalOrchestraMapping(std::shared_ptr<DataItem> item, std::string path)
      : item(item), path(std::move(path)),
        orchestraKey(0), orchestraBuffer(nullptr), orchestraTypeSize(0) {}

  void addSignal(Signal::Ptr signal, unsigned index) {
    if (!firstSignal) {
      villasOffset = index;
      firstSignal = signal;
    } else if (signal->type != lastSignal->type) {
      throw RuntimeError("Signal type mismatch: '{}' != '{}' for signal '{}'",
                         signalTypeToString(signal->type),
                         signalTypeToString(lastSignal->type), signal->name);
    } else if (signal->init.f != lastSignal->init.f) {
      throw RuntimeError("Signal default value mismatch for signal '{}'",
                         signal->name);
    }

    lastSignal = signal;
    villasLength++;
  }

  void check() {
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

    auto orchestraType = toOrchestraSignalType(firstSignal->type);
    if (item->type != orchestraType) {
      throw RuntimeError("Signal type mismatch: {} vs {} for signal '{}'",
                         signalTypeToString(item->type),
                         signalTypeToString(orchestraType), item->name);
    }
  }

  void prepare(unsigned int orchestraConnectionKey) {
    auto ret = RTGetInfoForItem(path.c_str(), &orchestraTypeSize, &orchestraLength);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get info for signal '{}'", item->name);
    }

    ret = RTGetKeyForItem(path.c_str(), &orchestraKey);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get key for signal '{}'", item->name);
    }

    ret = RTGetBuffer(orchestraConnectionKey, orchestraKey, (void **)&orchestraBuffer);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to get buffer for signal '{}'", item->name);
    }

    auto logger = Log::get("orchestra");
    logger->trace(
        "Prepared mapping: path='{}', type={}, orchestra.type_size={}, orchestra.key={}, orchestra.buffer={}"
        "orchestra.length={}, villas.length={}, villas.offset={}, default={}",
        path, orchestra::signalTypeToString(item->type), orchestraTypeSize, orchestraKey, orchestraBuffer,
        orchestraLength, villasLength, villasOffset, item->defaultValue);
  }

  void publish(struct Sample *smp) {
    unsigned length;

    length = MIN(villasLength, orchestraLength);
    length = MIN(length, smp->length);

    for (unsigned index = villasOffset; index < length; index++) {
      toOrchestraSignalData(orchestraBuffer + index * orchestraTypeSize, item->type, smp->data[villasOffset + index],
                            firstSignal->type);
    }
  }

  void subscribe(struct Sample *smp) {
    unsigned length;
    
    length = MIN(villasLength, orchestraLength);
    length = MIN(length, smp->capacity);

    for (unsigned index = 0; index < length; index++) {
      node::SignalType villasType;
      SignalData villasData =
          toNodeSignalData(orchestraBuffer + index * orchestraTypeSize, item->type, villasType);
      
      smp->data[villasOffset + index] = villasData.cast(villasType, firstSignal->type);
    }

    if (villasOffset + length > smp->length) {
      smp->length = villasOffset + length;
    }
  }
};

class OpalOrchestraNode : public Node {

protected:
  // The task which is used to pace the node in asynchronous mode.
  Task task;

  // A connection key identifies a connection between a specific combo of  Orchestra's framework and client.
  unsigned int connectionKey;
  unsigned int *status;

  // The domain to which the node belongs.
  Domain domain;

  std::unordered_map<std::shared_ptr<DataItem>, OpalOrchestraMapping>
      subscribeMappings;
  std::unordered_map<std::shared_ptr<DataItem>, OpalOrchestraMapping>
      publishMappings;

  double rate;
  std::optional<fs::path> dataDefinitionFilename;

  std::chrono::seconds connectTimeout;

  // Define a delay to wait, this will call the system function usleep and free the CPU.
  std::optional<std::chrono::microseconds> flagDelay;

  // Force the local Orchestra communication to be made with flag instead of semaphore when using an external communication process.
  std::optional<std::chrono::microseconds> flagDelayTool;

  // Skip wait-to-go step during start.
  bool skipWaitToGo;

  // Overwrite the data definition file (DDF).
  bool dataDefinitionFileOverwrite;

  int _read(struct Sample *smps[], unsigned cnt) override {
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

  void checkDuplicateNames() {
    for (auto &[name, item] : domain.publish.items) {
      if (domain.subscribe.items.find(name) != domain.subscribe.items.end()) {
        throw RuntimeError(
            "Orchestra signal '{}' is used for both publish and subscribe",
            name);
      }
    }
  }

  void checkMappings() {
    for (auto &mapping : subscribeMappings) {
      mapping.second.check();
    }

    for (auto &mapping : publishMappings) {
      mapping.second.check();
    }
  }

public:
  OpalOrchestraNode(const uuid_t &id = {}, const std::string &name = "",
                    unsigned int key = 0)
      : Node(id, name), task(), connectionKey(key), status(nullptr), domain(),
        subscribeMappings(), publishMappings(), rate(1000), connectTimeout(5),
        skipWaitToGo(false), dataDefinitionFileOverwrite(false) {}

  Signal::Ptr parseSignal(json_t *json_signal, NodeDirection::Direction dir, unsigned index) {
    auto signal = Signal::fromJson(json_signal);

    DataSet &dataSet =
        dir == NodeDirection::Direction::IN ? domain.publish : domain.subscribe;
    std::unordered_map<std::shared_ptr<DataItem>, OpalOrchestraMapping>
        &mappings = dir == NodeDirection::Direction::IN ? publishMappings
                                                        : subscribeMappings;

    const char *nme = nullptr;
    const char *typ = nullptr;
    int length = 1;

    json_error_t err;
    auto ret = json_unpack_ex(json_signal, &err, 0, "{ s?: i, s?: s, s?: s }",
                              "length", &length,"orchestra_name", &nme, "orchestra_type", &typ);
    if (ret) {
      throw ConfigError(json_signal, err,
                        "node-config-node-opal-orchestra-signals");
    }
    
    bool isVector = length > 1;

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
    } else if (!isVector) {
      throw ConfigError(json_signal, "node-config-node-opal-orchestra.signals", "Only vectors can have same Orchestra item name");
    }

    auto &mapping = mappings.at(item);
    mapping.addSignal(signal, index);

    return signal;
  }

  int parse(json_t *json) override {
    domain = Domain();
    publishMappings.clear();
    subscribeMappings.clear();

    int reti = parseCommon(
        json, [&](json_t *json_signal, NodeDirection::Direction dir, unsigned index) {
          return parseSignal(json_signal, dir, index);
        });
    if (reti)
      return reti;

    int sw = -1;
    int ow = -1;
    int sy = -1;
    int sts = -1;
    const char *dn = nullptr;
    const char *ddf = nullptr;
    json_t *json_connection = nullptr;
    json_t *json_connect_timeout = nullptr;
    json_t *json_flag_delay = nullptr;
    json_t *json_flag_delay_tool = nullptr;

    json_error_t err;
    auto ret = json_unpack_ex(
        json, &err, 0,
        "{ s: s, s?: b, s?: b, s?: o, s?: s, s?: o, s?: o, s?: o, s?: b, s?: "
        "b, s?: F }",
        "domain", &dn, "synchronous", &sy, "states", &sts, "connection",
        &json_connection, "ddf", &ddf, "connect_timeout", &json_connect_timeout,
        "flag_delay", &json_flag_delay, "flag_delay_tool",
        &json_flag_delay_tool, "skip_wait_to_go", &sw, "ddf_overwrite", &ow,
        "rate", &rate);
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

    return 0;
  }

  int check() override {
    if (dataDefinitionFilename) {
      if (!fs::exists(*dataDefinitionFilename) &&
          !dataDefinitionFileOverwrite) {
        throw RuntimeError("OPAL-RT Orchestra Data Definition file (DDF) at "
                           "{} does not exist",
                           *dataDefinitionFilename);
      }
    } else {
      if (dataDefinitionFileOverwrite) {
        throw RuntimeError(
            "The option 'ddf_overwrite' requires the option 'ddf' to be set");
      }
    }

    checkDuplicateNames();
    checkMappings();

    return Node::check();
  }

  int prepare() override {
    // Write DDF.
    if (dataDefinitionFilename && dataDefinitionFileOverwrite) {

      // TODO: Possibly merge Orchestra domains from all nodes into one DDF.
      auto ddf = DataDefinitionFile();
      ddf.domains.push_back(domain);
      ddf.writeToFile(*dataDefinitionFilename);

      logger->info("Wrote Orchestra Data Definition file (DDF) to '{}'",
                   *dataDefinitionFilename);
    }

    logger->debug("Connecting to Orchestra framework: domain={}, ddf={}, "
                  "connection_key={}",
                  domain.name,
                  dataDefinitionFilename ? *dataDefinitionFilename : "none",
                  connectionKey);

    RTConnectionLockGuard guard(connectionKey);

    auto ret = RTSetSkipWaitToGoAtConnection(skipWaitToGo);
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to check ready to go");
    }

    ret = dataDefinitionFilename
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
    RTConnectionLockGuard guard(connectionKey);

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
           (int)NodeFactory::Flags::SUPPORTS_POLL;
  }

  std::string getName() const override { return "opal.orchestra"; }

  std::string getDescription() const override {
    return "OPAL-RT Orchestra client";
  }

  int start(SuperNode *sn) override {
    const std::string opalBin = "/usr/opalrt/common/bin";

    // Append /usr/opalrt/common/bin to PATH on OPAL-RT Linux systems
    // for finding OrchestraExtComm tools.
    std::string path = getenv("PATH");

    if (path.find(opalBin) == std::string::npos) {
      path = path + ":" + opalBin;

      auto ret = setenv("PATH", path.c_str(), 1);
      if (ret != 0) {
        throw RuntimeError("Failed to set PATH environment variable");
      }
    }

    return 0;
  }
};

static OpalOrchestraNodeFactory p;
