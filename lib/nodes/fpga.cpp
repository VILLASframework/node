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
#include <villas/nodes/fpga.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

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
    : Node(id, name), cardName(""), connectStrings(), card(nullptr), dma(),
      blockRx(), blockTx(), readActive(false), writeActive(false),
      stopThreads(false), dmaThread() {}

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

  dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
      card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
  if (dma == nullptr) {
    logger->error("No DMA found on FPGA ");
    throw std::runtime_error("No DMA found on FPGA");
  }

  auto &alloc = HostRam::getAllocator();

  blockRx = alloc.allocateBlock(0x200 * sizeof(float));
  blockTx = alloc.allocateBlock(0x200 * sizeof(float));
  villas::MemoryAccessor<float> memRx = *blockRx;
  villas::MemoryAccessor<float> memTx = *blockTx;

  dma->makeAccesibleFromVA(blockRx);
  dma->makeAccesibleFromVA(blockTx);

  MemoryManager::get().printGraph();

  return Node::prepare();
}

int FpgaNode::stop() {
  if (asyncDmaManagement) {
    stopThreads = true;
    if (dmaThread) {
      dmaThread->join();
    }
  }
  return Node::stop();
}

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

  ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: b}", "card",
                       &jsonCard, "connect", &jsonConnectStrings,
                       "asyncDmaManagement", &asyncDmaManagement);
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

    details = fmt::format("fpga={}, connect={}", name, imploded.str());
  }

  return details;
}

int FpgaNode::check() { return 0; }

int FpgaNode::start() {
  if (asyncDmaManagement) {
    dmaThread = std::make_shared<std::thread>(&FpgaNode::dmaMgmtThread, this);
  }
  dma->read(*blockRx, blockRx->getSize());
  return Node::start();
}

int FpgaNode::dmaMgmtThread() {
  while (readActive) {
    usleep(1);
  }
  while (!stopThreads) {
    // readActive must be true, writeActive must be false
    dma->read(*blockRx, blockRx->getSize());
    readActive = true;
    while (readActive && !stopThreads) {
    }
    while (!writeActive && !stopThreads) {
    }
    // readActive must be false, writeActive must be true
    dma->writeComplete();
    writeActive = false;
  }

  return 0;
}

int FpgaNode::_read(Sample *smps[], unsigned cnt) {
  unsigned read;
  Sample *smp = smps[0];

  assert(cnt == 1);

  if (asyncDmaManagement) {
    while (!readActive.load(std::memory_order_relaxed) && !stopThreads)
      ;
  } else {
    // dma->read(*blockRx, blockRx->getSize());
  }
  auto c = dma->readComplete();
  if (asyncDmaManagement) {
    readActive.store(false, std::memory_order_relaxed);
  }

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
  // unsigned int written;
  Sample *smp = smps[0];

  assert(cnt == 1 && smps != nullptr && smps[0] != nullptr);

  if (asyncDmaManagement) {
    while (writeActive.load(std::memory_order_relaxed) && !stopThreads)
      ;
  }
  auto mem = MemoryAccessor<uint32_t>(*blockTx);
  float scaled;

  for (unsigned i = 0; i < smps[0]->length; i++) {
    scaled = smps[0]->data[i].f;
    if (scaled > 10.) {
      scaled = 10.;
    } else if (scaled < -10.) {
      scaled = -10.;
    }
    mem[i] = (scaled + 10.) * ((float)0xFFFF / 20.);
  }

  bool state = dma->write(*blockTx, smp->length * sizeof(float));
  if (!state) {
    return -1;
  }
  if (asyncDmaManagement) {
    writeActive.store(true, std::memory_order_relaxed);
  } else {
    auto written = dma->writeComplete().bytes /
                   sizeof(float); // The number of samples written

    if (written != smp->length) {
      logger->warn("Wrote {} samples, but {} were expected", written,
                   smp->length);
    }
    dma->read(*blockRx, blockRx->getSize());
  }

  return 1;
}

std::vector<int> FpgaNode::getPollFDs() {
  return card->vfioDevice->getEventfdList();
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
