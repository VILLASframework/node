/* DMA driver
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <villas/config.hpp>
#include <villas/exceptions.hpp>
#include <villas/fpga/node.hpp>
#include <villas/memory.hpp>
#include <xilinx/xaxidma.h>

namespace villas {
namespace fpga {
namespace ip {

class Dma : public Node {
public:
  friend class DmaFactory;

  virtual ~Dma();

  virtual bool init() override;

  bool reset() override;

  // Memory-mapped to stream (MM2S)
  bool write(const MemoryBlock &mem, size_t len);

  // Stream to memory-mapped (S2MM)
  bool read(const MemoryBlock &mem, size_t len);

  struct Completion {
    Completion() : bytes(0), bds(0), interrupts(0) {}
    size_t bytes; // Number of bytes transferred
    size_t bds;   // Number of buffer descriptors used (only for scatter-gather)
    size_t
        interrupts; // Number of interrupts received since last call (only if interrupts enabled)
  };

  Completion writeComplete() {
    return hasScatterGather() ? writeCompleteScatterGather()
                              : writeCompleteSimple();
  }

  bool readScatterGatherPrepare(const MemoryBlock &mem, size_t len);
  bool readScatterGatherFast();
  size_t readScatterGatherPoll(bool lock = true);

  bool writeScatterGatherPrepare(const MemoryBlock &mem, size_t len);
  bool writeScatterGatherFast();
  size_t writeScatterGatherPoll(bool lock = true);

  Completion readComplete() {
    return hasScatterGather() ? readCompleteScatterGather()
                              : readCompleteSimple();
  }

  bool memcpy(const MemoryBlock &src, const MemoryBlock &dst, size_t len);

  void makeAccesibleFromVA(std::shared_ptr<MemoryBlock> mem);
  bool makeInaccesibleFromVA(const MemoryBlock &mem);

  inline bool hasScatterGather() const { return xConfig.HasSg; }

  const StreamVertex &getDefaultSlavePort() const override {
    return getSlavePort(s2mmPort);
  }

  const StreamVertex &getDefaultMasterPort() const override {
    return getMasterPort(mm2sPort);
  }

  static constexpr const char *s2mmPort = "S2MM";
  static constexpr const char *mm2sPort = "MM2S";

  bool isMemoryBlockAccesible(const MemoryBlock &mem,
                              const std::string &interface);

  virtual void dump() override;

private:
  bool writeScatterGather(const void *buf, size_t len);
  bool readScatterGather(void *buf, size_t len);
  XAxiDma_Bd *writeScatterGatherSetupBd(const void *buf, size_t len);
  Completion writeCompleteScatterGather();
  XAxiDma_Bd *readScatterGatherSetupBd(void *buf, size_t len);
  Completion readCompleteScatterGather();

  bool writeSimple(const void *buf, size_t len);
  bool readSimple(void *buf, size_t len);
  Completion writeCompleteSimple();
  Completion readCompleteSimple();

  void setupScatterGather();
  void setupScatterGatherRingRx(uintptr_t physAddr, uintptr_t virtAddr);
  void setupScatterGatherRingTx(uintptr_t physAddr, uintptr_t virtAddr);

  static constexpr char registerMemory[] = "Reg";

  static constexpr char mm2sInterrupt[] = "mm2s_introut";
  static constexpr char mm2sInterface[] = "M_AXI_MM2S";

  static constexpr char s2mmInterrupt[] = "s2mm_introut";
  static constexpr char s2mmInterface[] = "M_AXI_S2MM";

  // Optional Scatter-Gather interface to access descriptors
  static constexpr char sgInterface[] = "M_AXI_SG";

  std::list<MemoryBlockName> getMemoryBlocks() const override {
    return {registerMemory};
  }

  XAxiDma xDma;
  XAxiDma_Config xConfig;

  std::mutex hwLock;

  bool configDone = false;
  // use polling to wait for DMA completion or interrupts via efds
  bool polling = false; // polling mode is significantly lower latency
  bool cyclic = false;  // not fully implemented
  // Timeout after which the DMA controller issues in interrupt if no data has been received
  // Delay is 125 x <delay> x (clock period of SG clock). SG clock is 100 MHz by default.
  int delay = 0;
  // Coalesce is the number of messages/BDs to wait for before issuing an interrupt
  uint32_t writeCoalesce = 1;
  uint32_t readCoalesce = 1;

  // (maximum) size of a single message on the read channel in bytes.
  // The message buffer/BD should have enough room for this many bytes.
  size_t readMsgSize = 4;

  // When using SG: ringBdSize is the maximum number of BDs usable in the ring
  // Depending on alignment, the actual number of BDs usable can be smaller
  // We use a single BD for transfers, because this way we can achieve the best
  // latency. The AXI read cache in the FPGA also only supports a single BD.
  // TODO: We could make this configurable in the future.
  static constexpr size_t requestedRingBdSize = 1;
  static constexpr size_t requestedRingBdSizeMemory =
      requestedRingBdSize * sizeof(XAxiDma_Bd);
  uint32_t actualRingBdSize = 1;
  std::shared_ptr<MemoryBlock> sgRing;
};

class DmaFactory : NodeFactory {

public:
  virtual std::string getName() const override { return "dma"; }

  virtual std::string getDescription() const override {
    return "Xilinx's AXI4 Direct Memory Access Controller";
  }

private:
  virtual Vlnv getCompatibleVlnv() const override {
    return Vlnv("xilinx.com:ip:axi_dma:");
  }

  // Create a concrete IP instance
  Core *make() const override { return new Dma; };

protected:
  virtual void parse(Core &ip, json_t *json) override;

  virtual void configurePollingMode(Core &ip, PollingMode mode) override {
    dynamic_cast<Dma &>(ip).polling = (mode == POLL);
  }
};

} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::Dma> : public fmt::ostream_formatter {};
#endif
