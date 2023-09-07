/* Communicate with VILLASfpga Xilinx FPGA boards.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
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

#define FPGA_DMA_VLNV
#define FPGA_AURORA_VLNV "acs.eonerc.rwth-aachen.de:user:aurora_axis:"

class FpgaNode : public Node {

protected:
  int irqFd;
  int coalesce;
  bool polling;

  std::shared_ptr<fpga::PCIeCard> card;

  std::shared_ptr<fpga::ip::Dma> dma;
  std::shared_ptr<fpga::ip::Node> intf;

  std::unique_ptr<const MemoryBlock> blockRx;
  std::unique_ptr<const MemoryBlock> blockTx;

  // Config only
  std::string cardName;
  std::string intfName;
  std::string dmaName;

protected:
  virtual int _read(Sample *smps[], unsigned cnt);

  virtual int _write(Sample *smps[], unsigned cnt);

public:
  FpgaNode(const uuid_t &id = {}, const std::string &name = "");

  virtual ~FpgaNode();

  virtual int parse(json_t *json);

  virtual const std::string &getDetails();

  virtual int check();

  virtual int prepare();

  virtual std::vector<int> getPollFDs();
};

class FpgaNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  virtual Node *make(const uuid_t &id = {}, const std::string &nme = "") {
    auto *n = new FpgaNode(id, nme);

    init(n);

    return n;
  }

  virtual int getFlags() const {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL;
  }

  virtual std::string getName() const { return "fpga"; }

  virtual std::string getDescription() const { return "VILLASfpga"; }

  virtual int start(SuperNode *sn);
};

} // namespace node
} // namespace villas
