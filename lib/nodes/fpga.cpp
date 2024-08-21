/* Communicate with VILLASfpga Xilinx FPGA boards.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <jansson.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/nodes/fpga.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/ips/dino.hpp>
#include <villas/fpga/ips/register.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::fpga;
using namespace villas::utils;

// Global state
static std::list<std::shared_ptr<fpga::Card>> cards;

static std::shared_ptr<kernel::vfio::Container> vfioContainer;

FpgaNode::FpgaNode(const uuid_t &id, const std::string &name)
    : Node(id, name), cardName(""), connectStrings(), lowLatencyMode(false),
      timestep(10e-3), card(nullptr), dma(), blockRx(), accessorRx(nullptr),
      blockTx(), accessorTx(nullptr) {}

FpgaNode::~FpgaNode() {}

int FpgaNode::prepare() {
  if (card == nullptr) {
    for (auto &fpgaCard : cards) {
      if (fpgaCard->name == cardName) {
        card = fpgaCard;
        break;
      }
    }
    if (card == nullptr) {
      throw ConfigError(config, "node-config-fpga",
                        "There is no FPGA card with the name: {}", cardName);
    }
  }

  auto axisswitch = std::dynamic_pointer_cast<fpga::ip::AxiStreamSwitch>(
      card->lookupIp(fpga::Vlnv("xilinx.com:ip:axis_switch:")));

  for (auto ports : axisswitch->getMasterPorts()) {
    logger->info("Port: {}, {}", ports.first, *ports.second);
  }
  for (auto ports : axisswitch->getSlavePorts()) {
    logger->info("Port: {}, {}", ports.first, *ports.second);
  }
  // Configure Crossbar switch
  for (std::string str : connectStrings) {
    const fpga::ConnectString parsedConnectString(str);
    parsedConnectString.configCrossBar(card);
  }

  auto reg = std::dynamic_pointer_cast<fpga::ip::Register>(
      card->lookupIp(fpga::Vlnv("xilinx.com:module_ref:registerif:")));

  if (reg != nullptr &&
      card->lookupIp(fpga::Vlnv("xilinx.com:module_ref:dinoif_fast:"))) {
    fpga::ip::DinoAdc::setRegisterConfigTimestep(reg, timestep);
  } else {
    logger->warn("No DinoAdc or no Register found on FPGA.");
  }

  dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
      card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
  if (dma == nullptr) {
    logger->error("No DMA found on FPGA ");
    throw std::runtime_error("No DMA found on FPGA");
  }

  auto &alloc = HostRam::getAllocator();

  blockRx = alloc.allocateBlock(0x200 * sizeof(float));
  blockTx = alloc.allocateBlock(0x200 * sizeof(float));
  accessorRx = std::make_shared<MemoryAccessor<uint32_t>>(*blockRx);
  accessorTx = std::make_shared<MemoryAccessor<uint32_t>>(*blockTx);

  dma->makeAccesibleFromVA(blockRx);
  dma->makeAccesibleFromVA(blockTx);

  MemoryManager::get().printGraph();

  return Node::prepare();
}

int FpgaNode::stop() { return Node::stop(); }

int FpgaNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret) {
    return ret;
  }

  json_error_t err;

  json_t *jsonCard = nullptr;
  json_t *jsonConnectStrings = nullptr;

  if (vfioContainer == nullptr) {
    vfioContainer = std::make_shared<kernel::vfio::Container>();
  }

  ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: b, s?: f}", "card",
                       &jsonCard, "connect", &jsonConnectStrings,
                       "low_latency_mode", &lowLatencyMode, "timestep",
                       &timestep);
  if (ret) {
    throw ConfigError(json, err, "node-config-fpga",
                      "Failed to parse configuration of node {}",
                      this->getName());
  }
  if (json_is_string(jsonCard)) {
    cardName = std::string(json_string_value(jsonCard));
  } else if (json_is_object(jsonCard)) {
    json_t *jsonCardName = json_object_get(jsonCard, "name");
    if (jsonCardName != nullptr && !json_is_string(jsonCardName)) {
      cardName = std::string(json_string_value(jsonCardName));
    } else {
      cardName = "anonymous Card";
    }
    auto searchPath = configPath.substr(0, configPath.rfind("/"));
    card = createCard(jsonCard, searchPath, vfioContainer, cardName);
    if (card == nullptr) {
      throw ConfigError(jsonCard, "node-config-fpga", "Failed to create card");
    } else {
      cards.push_back(card);
    }
  } else {
    throw ConfigError(jsonCard, "node-config-fpga",
                      "Failed to parse card name");
  }

  if (jsonConnectStrings != nullptr && json_is_array(jsonConnectStrings)) {
    for (size_t i = 0; i < json_array_size(jsonConnectStrings); i++) {
      json_t *jsonConnectString = json_array_get(jsonConnectStrings, i);
      if (jsonConnectString == nullptr || !json_is_string(jsonConnectString)) {
        throw ConfigError(jsonConnectString, "node-config-fpga",
                          "Failed to parse connect string");
      }
      connectStrings.push_back(
          std::string(json_string_value(jsonConnectString)));
    }
  }

  return 0;
}

const std::string &FpgaNode::getDetails() {
  if (details.empty()) {
    auto &name = card ? card->name : cardName;

    const char *const delim = ", ";

    std::ostringstream imploded;
    std::copy(connectStrings.begin(), connectStrings.end(),
              std::ostream_iterator<std::string>(imploded, delim));

    details = fmt::format("fpga={}, connect={}, lowLatencyMode={}", name,
                          imploded.str(), lowLatencyMode);
  }

  return details;
}

int FpgaNode::check() { return Node::check(); }

int FpgaNode::start() {
  if (getOutputSignalsMaxCount() * sizeof(float) > blockTx->getSize()) {
    logger->error("Output signals exceed block size.");
    throw villas ::RuntimeError("Output signals exceed block size.");
  }
  if (getInputSignalsMaxCount() * sizeof(float) > blockRx->getSize()) {
    logger->error("Output signals exceed block size.");
    throw villas ::RuntimeError("Output signals exceed block size.");
  }
  if (lowLatencyMode) {
    if (getInputSignalsMaxCount() != 0) {
      dma->readScatterGatherPrepare(*blockRx,
                                    getInputSignalsMaxCount() * sizeof(float));
    } else {
      logger->warn("No input signals defined. Not preparing read buffer - "
                   "reads will not work.");
    }
    if (getOutputSignalsMaxCount() != 0) {
      dma->writeScatterGatherPrepare(*blockTx, getOutputSignalsMaxCount() *
                                                   sizeof(float));
    } else {
      logger->warn("No output signals defined. Not preparing write buffer - "
                   "writes will not work.");
    }
  }

  return Node::start();
}

// We cannot modify the BD here, so writes are fixed length.
// If fastWrite receives less signals than expected, the previous data
// will be reused for the remaining signals
int FpgaNode::fastWrite(Sample *smps[], unsigned cnt) {
  Sample *smp = smps[0];

  assert(cnt == 1 && smps != nullptr && smps[0] != nullptr);

  for (unsigned i = 0; i < smp->length; i++) {
    if (smp->signals->getByIndex(i)->type == SignalType::FLOAT) {
      float f = static_cast<float>(smp->data[i].f);
      (*accessorTx)[i] = *reinterpret_cast<uint32_t *>(&f);
    } else {
      (*accessorTx)[i] = static_cast<uint32_t>(smp->data[i].i);
    }
  }

  logger->trace("Writing sample: {}, {}, {:#x}", smp->data[0].f,
                signalTypeToString(smp->signals->getByIndex(0)->type),
                smp->data[0].i);
  dma->writeScatterGatherFast();
  auto written = dma->writeScatterGatherPoll() /
                 sizeof(float); // The number of samples written

  if (written != smp->length) {
    logger->warn("Wrote {} samples, but {} were expected", written,
                 smp->length);
  }

  return 1;
}

// Because we cannot modify the BD here, reads are fixed length.
// However, if we receive less data than expected, we will return only
// what we have received. fastRead is thus capable of partial reads.
int FpgaNode::fastRead(Sample *smps[], unsigned cnt) {
  Sample *smp = smps[0];

  smp->flags = (int)SampleFlags::HAS_DATA;
  smp->signals = in.signals;

  size_t to_read = in.signals->size() * sizeof(uint32_t);
  size_t read;
  do {
    dma->readScatterGatherFast();
    read = dma->readScatterGatherPoll(true);
    if (read < to_read) {
      logger->warn("Read only {} bytes, but {} were expected", read, to_read);
    }
  } while (read < to_read);
  // when we read less than expected we don't know what we missed so the data is
  // useless. Thus, we discard what we read and try again.

  // We assume a lot without checking at this point. All for the latency!

  smp->length = 0;
  for (unsigned i = 0; i < MIN(read / sizeof(uint32_t), smp->capacity); i++) {
    if (i >= in.signals->size()) {
      logger->warn(
          "Received more data than expected. Maybe the descriptor cache needs "
          "to be invalidated?. Ignoring the rest of the data.");
      break;
    }
    if (in.signals->getByIndex(i)->type == SignalType::INTEGER) {
      smp->data[i].i = static_cast<int64_t>((*accessorRx)[i]);
    } else {
      smp->data[i].f =
          static_cast<double>(*reinterpret_cast<float *>(&(*accessorRx)[i]));
    }
    smp->length++;
  }

  return 1;
}

int FpgaNode::_read(Sample *smps[], unsigned cnt) {
  if (lowLatencyMode) {
    return fastRead(smps, cnt);
  } else {
    return slowRead(smps, cnt);
  }
}

int FpgaNode::slowRead(Sample *smps[], unsigned cnt) {
  unsigned read;
  Sample *smp = smps[0];

  assert(cnt == 1);

  dma->read(*blockRx, blockRx->getSize());
  auto c = dma->readComplete();

  read = c.bytes / sizeof(float);

  if (c.interrupts > 1) {
    logger->warn("Missed {} interrupts", c.interrupts - 1);
  }

  auto mem = MemoryAccessor<float>(*blockRx);

  smp->length = 0;
  for (unsigned i = 0; i < MIN(read, smp->capacity); i++) {
    smp->data[i].f = static_cast<double>(mem[i]);
    smp->length++;
  }
  smp->flags = (int)SampleFlags::HAS_DATA;

  smp->signals = in.signals;

  return 1;
}

int FpgaNode::_write(Sample *smps[], unsigned cnt) {
  if (lowLatencyMode) {
    return fastWrite(smps, cnt);
  } else {
    return slowWrite(smps, cnt);
  }
}

int FpgaNode::slowWrite(Sample *smps[], unsigned cnt) {
  // unsigned int written;
  Sample *smp = smps[0];

  assert(cnt == 1 && smps != nullptr && smps[0] != nullptr);

  auto mem = MemoryAccessor<float>(*blockTx);

  for (unsigned i = 0; i < smps[0]->length; i++) {
    mem[i] = smps[0]->data[i].f;
  }

  bool state = dma->write(*blockTx, smp->length * sizeof(float));
  if (!state) {
    return -1;
  }
  auto written = dma->writeComplete().bytes /
                 sizeof(float); // The number of samples written

  if (written != smp->length) {
    logger->warn("Wrote {} samples, but {} were expected", written,
                 smp->length);
  }

  return 1;
}

std::vector<int> FpgaNode::getPollFDs() {
  if (!lowLatencyMode && card && !card->polling) {
    return card->vfioDevice->getEventfdList();
  } else {
    return {};
  }
}

int FpgaNodeFactory::start(SuperNode *sn) {
  if (vfioContainer == nullptr) {
    vfioContainer = std::make_shared<kernel::vfio::Container>();
  }

  if (cards.empty()) {
    auto searchPath =
        sn->getConfigPath().substr(0, sn->getConfigPath().rfind("/"));
    createCards(sn->getConfig(), cards, searchPath, vfioContainer);
  }

  return NodeFactory::start(sn);
}

static FpgaNodeFactory p;
