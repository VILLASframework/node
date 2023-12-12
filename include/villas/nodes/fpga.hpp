/* Communicate with VILLASfpga Xilinx FPGA boards.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/timing.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/pcie_card.hpp>

namespace villas {
namespace node {

class FpgaNode : public Node {

  enum InterfaceType { PCIE, PLATFORM };

protected:
  // Settings
  std::string cardName;
  std::list<std::string> connectStrings;

  // State
  std::shared_ptr<fpga::Card> card;
  std::shared_ptr<villas::fpga::ip::Dma> dma;
  std::shared_ptr<villas::MemoryBlock> blockRx[2];
  std::shared_ptr<villas::MemoryBlock> blockTx;

  // Non-public methods
  virtual int _read(Sample *smps[], unsigned cnt) override;

  virtual int _write(Sample *smps[], unsigned cnt) override;

public:
  FpgaNode(const uuid_t &id = {}, const std::string &name = "");

  virtual ~FpgaNode();

  virtual int prepare() override;

  virtual int parse(json_t *json) override;

  virtual int check() override;

  virtual int start() override;

  virtual std::vector<int> getPollFDs() override;

  virtual const std::string &getDetails() override;
};

class FpgaNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  virtual Node *make(const uuid_t &id = {},
                     const std::string &nme = "") override {
    auto *n = new FpgaNode(id, nme);

    init(n);

    return n;
  }

  virtual int getFlags() const override {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL;
  }

  virtual std::string getName() const override { return "fpga"; }

  virtual std::string getDescription() const override { return "VILLASfpga"; }

  virtual int start(SuperNode *sn) override;
};

} // namespace node
} // namespace villas
