/* Matcher for IP devices in an OS devicetree and ips on a fpga board.
 * Matching based on address prefix in device name and ip memory address
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/core.hpp>
#include <villas/kernel/devices/ip_device.hpp>

class DeviceIpMatcher {
private:
  std::vector<villas::kernel::devices::IpDevice> devices;
  std::list<std::shared_ptr<villas::fpga::ip::Core>> ips;

public:
  DeviceIpMatcher(std::vector<villas::kernel::devices::IpDevice> devices,
                  std::list<std::shared_ptr<villas::fpga::ip::Core>> ips)
      : devices(devices), ips(ips) {}

  std::vector<std::pair<std::shared_ptr<villas::fpga::ip::Core>,
                        villas::kernel::devices::IpDevice>>
  match() const {
    std::vector<std::pair<std::shared_ptr<villas::fpga::ip::Core>,
                          villas::kernel::devices::IpDevice>>
        pairs;
    for (auto device : devices) {
      for (auto ip : ips) {
        if (ip->getBaseaddr() == device.addr()) {
          pairs.push_back(std::make_pair(ip, device));
        }
      }
    }
    return pairs;
  }
};