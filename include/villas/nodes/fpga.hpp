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

#include <stdint.h>
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

  // This setting improves latency by removing various checks.
  // Use with caution! Requires read cache in FPGA design!
  // The common use case in VILLASfpga is that we have exactly
  // one write for every read and the number of exchanged signals
  // do not change. If this is the case, we can reuse the buffer
  // descriptors during reads and write, thus avoidng freeing,
  // reallocating and setting them up.
  // We set up the descriptors in start, and in write or read,
  // we only reset the complete bit in the buffer descriptor and
  // write to the tdesc register to start the DMA transfer.
  // Improves read/write latency by approx. 40%.
  bool lowLatencyMode;

  // This sets the requested timestep in the FPGA timestep generation IP.
  // The value is interpreted as seconds (i.e. 10e-3 is a timestep of 10ms).
  // This does not generate a clock, but a pulse at a configurable interval
  // that is (mostly) independent from the actual clock. If the FPGA is
  // coupled with a simulation, this should be the actual time step, i.e.,
  // we generate the time step signal in FPGA and synchronize our simulator
  // software to that signal.
  // This setting is also the inverse of the Dino sampling rate if one is connected.
  // For this, the setting must be programmed using DinoAdc::setRegisterConfigTimestep,
  // which FpgaNode does in prepare, but only if a Register and a DinoAdc is present.
  double timestep;

  // State
  std::shared_ptr<fpga::Card> card;
  std::shared_ptr<villas::fpga::ip::Dma> dma;
  std::shared_ptr<villas::MemoryBlock> blockRx;
  std::shared_ptr<MemoryAccessor<uint32_t>> accessorRxInt;
  std::shared_ptr<MemoryAccessor<float>> accessorRxFloat;
  std::shared_ptr<villas::MemoryBlock> blockTx;
  std::shared_ptr<MemoryAccessor<uint32_t>> accessorTxInt;
  std::shared_ptr<MemoryAccessor<float>> accessorTxFloat;

  // Non-public methods
  virtual int fastRead(Sample *smps[], unsigned cnt);
  virtual int slowRead(Sample *smps[], unsigned cnt);
  virtual int _read(Sample *smps[], unsigned cnt) override;
  virtual int fastWrite(Sample *smps[], unsigned cnt);
  virtual int slowWrite(Sample *smps[], unsigned cnt);
  virtual int _write(Sample *smps[], unsigned cnt) override;

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
