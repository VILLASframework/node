/* GPU2RTDS IP core
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/ips/hls.hpp>
#include <villas/fpga/ips/rtds2gpu/register_types.hpp>
#include <villas/fpga/ips/rtds2gpu/xgpu2rtds_hw.h>
#include <villas/fpga/node.hpp>
#include <villas/memory.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Gpu2Rtds : public Node, public Hls {
public:
  friend class Gpu2RtdsFactory;

  virtual bool init() override;

  void dump(spdlog::level::level_enum logLevel = spdlog::level::info);

  void dump() override { dump(spdlog::level::info); }

  bool startOnce(size_t frameSize);

  size_t getMaxFrameSize();

  const StreamVertex &getDefaultMasterPort() const {
    return getMasterPort(rtdsOutputStreamPort);
  }

  MemoryBlock getRegisterMemory() const {
    return MemoryBlock(0, 1 << 10, getAddressSpaceId(registerMemory));
  }

private:
  bool updateStatus();

public:
  static constexpr const char *rtdsOutputStreamPort = "rtds_output";

  struct StatusControlRegister {
    uint32_t status_ap_vld : 1, _res : 31;
  };

  using StatusRegister = axilite_reg_status_t;

  static constexpr uintptr_t registerStatusOffset =
      XGPU2RTDS_CTRL_ADDR_STATUS_DATA;
  static constexpr uintptr_t registerStatusCtrlOffset =
      XGPU2RTDS_CTRL_ADDR_STATUS_CTRL;
  static constexpr uintptr_t registerFrameSizeOffset =
      XGPU2RTDS_CTRL_ADDR_FRAME_SIZE_DATA;
  static constexpr uintptr_t registerFrameOffset =
      XGPU2RTDS_CTRL_ADDR_FRAME_BASE;
  static constexpr uintptr_t registerFrameLength = XGPU2RTDS_CTRL_DEPTH_FRAME;

public:
  StatusRegister *registerStatus;
  StatusControlRegister *registerStatusCtrl;
  uint32_t *registerFrameSize;
  uint32_t *registerFrames;

  size_t maxFrameSize;

  bool started;
};

} // namespace ip
} // namespace fpga
} // namespace villas
