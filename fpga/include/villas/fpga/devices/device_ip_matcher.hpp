/* Devicematcher
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/core.hpp>
#include <villas/fpga/devices/ip_device.hpp>

class DeviceIpMatcher {
private:
  std::vector<IpDevice> devices;
  std::list<std::shared_ptr<villas::fpga::ip::Core>> ips;

public:
  DeviceIpMatcher(std::vector<IpDevice> devices,
                  std::list<std::shared_ptr<villas::fpga::ip::Core>> ips)
      : devices(devices), ips(ips) {}

  std::vector<std::pair<std::shared_ptr<villas::fpga::ip::Core>, IpDevice>>
  match() const {
    std::vector<std::pair<std::shared_ptr<villas::fpga::ip::Core>, IpDevice>>
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