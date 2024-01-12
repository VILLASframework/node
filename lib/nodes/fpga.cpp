/* Communicate with VILLASfpga Xilinx FPGA boards.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <memory>
#include <string>
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
    : Node(id, name), cardName(""), card(), dma(), blockRx(), blockTx() {}

FpgaNode::~FpgaNode() {}

int FpgaNode::prepare() {
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

  auto &alloc = HostRam::getAllocator();

  blockRx[0] = alloc.allocateBlock(0x200 * sizeof(float));
  blockRx[1] = alloc.allocateBlock(0x200 * sizeof(float));
  blockTx = alloc.allocateBlock(0x200 * sizeof(float));
  villas::MemoryAccessor<float> memRx[] = {*(blockRx[0]), *(blockRx[1])};
  villas::MemoryAccessor<float> memTx = *blockTx;

  dma->makeAccesibleFromVA(blockRx[0]);
  dma->makeAccesibleFromVA(blockRx[1]);
  dma->makeAccesibleFromVA(blockTx);

  return Node::prepare();
}

int FpgaNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  json_error_t err;

  const char *jsonCardName = nullptr;
  json_t *jsonConnectStrings = nullptr;

  ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: o}", "card", &jsonCardName,
                       "connect", &jsonConnectStrings);
  if (ret) {
    throw ConfigError(json, err, "node-config-fpga",
                      "Failed to parse configuration of node {}",
                      this->getName());
  }
  cardName = std::string(jsonCardName);
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
  // enque first read
  dma->read(*(blockRx[0]), blockRx[0]->getSize());
  return Node::start();
}

int FpgaNode::_read(Sample *smps[], unsigned cnt) {
  static size_t cur = 0, next = 1;
  unsigned read;
  Sample *smp = smps[0];

  assert(cnt == 1);

  dma->read(*(blockRx[next]), blockRx[next]->getSize()); // TODO: calc size
  auto c = dma->readComplete();
  read = c.bytes / sizeof(float);

  if (c.interrupts > 1) {
    logger->warn("Missed {} interrupts", c.interrupts - 1);
  }

  auto mem = MemoryAccessor<float>(*(blockRx[cur]));

  smp->length = 0;
  for (unsigned i = 0; i < MIN(read, smp->capacity); i++) {
    smp->data[i].f = static_cast<double>(mem[i]);
    smp->length++;
  }
  smp->flags = (int)SampleFlags::HAS_DATA;

  smp->signals = in.signals;
  cur = next;
  next = (next + 1) % (sizeof(blockRx) / sizeof(blockRx[0]));

  return 1;
}

int FpgaNode::_write(Sample *smps[], unsigned cnt) {
  unsigned int written;
  Sample *smp = smps[0];

  assert(cnt == 1 && smps != nullptr && smps[0] != nullptr);

  auto mem = MemoryAccessor<float>(*blockTx);

  for (unsigned i = 0; i < smps[0]->length; i++) {
    mem[i] = static_cast<float>(smps[0]->data[i].f);
  }

  bool state = dma->write(*blockTx, smp->length * sizeof(float));
  if (!state)
    return -1;

  written = dma->writeComplete().bytes /
            sizeof(float); // The number of samples written

  if (written != smp->length) {
    logger->warn("Wrote {} samples, but {} were expected", written,
                 smp->length);
  }

  return 1;
}

std::vector<int> FpgaNode::getPollFDs() {
  return card->vfioDevice->getEventfdList();
}

int FpgaNodeFactory::start(SuperNode *sn) {
  vfioContainer = std::make_shared<kernel::vfio::Container>();

  if (cards.empty()) {
    std::filesystem::path searchPath = sn->getConfigPath().parent_path();
    createCards(sn->getConfig(), cards, searchPath);
  }

  return NodeFactory::start(sn);
}

static FpgaNodeFactory p;
