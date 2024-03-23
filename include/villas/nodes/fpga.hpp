/* Communicate with VILLASfpga Xilinx FPGA boards.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <thread>
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

  // This setting decouples DMA management from Data processing.
  // With this setting set to true, the DMA management for both read and
  // write transactions is performed after the write command has been send
  // the DMA controller.
  // This allows us to achieve very low latencies for an application that
  // waits for data from the FPGA processes it, and finished a time step
  // by issuing a write to the FPGA.
  bool lowLatencyMode;
  // This setting performs synchronization with DMA controller in separate
  // threads. It requires lowLatencyMode to be set to true.
  // This may improve latency, because DMA management is completely decoupled
  // from the data path, or may increase latency because of additional thread
  // synchronization overhead. Only use after verifying that it improves latency.
  bool asyncDmaManagement;

  // State
  std::shared_ptr<fpga::Card> card;
  std::shared_ptr<villas::fpga::ip::Dma> dma;
  std::shared_ptr<villas::MemoryBlock> blockRx;
  std::shared_ptr<villas::MemoryBlock> blockTx;

  // Non-public methods
  virtual int asyncRead(Sample *smps[], unsigned cnt);
  virtual int slowRead(Sample *smps[], unsigned cnt);
  virtual int _read(Sample *smps[], unsigned cnt) override;
  virtual int _write(Sample *smps[], unsigned cnt) override;

  // only used if asyncDmaManagement is true
  volatile std::atomic_bool readActive;
  volatile std::atomic_bool writeActive;
  volatile std::atomic_bool stopThreads;
  std::shared_ptr<std::thread> dmaThread;
  virtual int dmaMgmtThread();

public:
  FpgaNode(const uuid_t &id = {}, const std::string &name = "");

  virtual ~FpgaNode();

  virtual int prepare() override;

  virtual int parse(json_t *json) override;

  virtual int check() override;

  virtual int start() override;

  virtual int stop() override;

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
