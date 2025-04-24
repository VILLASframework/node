/* Platform based card
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 *
 * SPDX-FileCopyrightText: 2023-2025 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <vector>

#include <villas/fpga/card.hpp>
#include <villas/fpga/core_connection.hpp>

namespace villas {
namespace fpga {

class PlatformCard : public Card {
public:
  PlatformCard(std::shared_ptr<kernel::vfio::Container> vfioContainer);

  ~PlatformCard(){};

  std::vector<CoreConnection> core_connections;

  void
  connectVFIOtoIps(const std::list<std::shared_ptr<ip::Core>> &configuredIps);
  bool mapMemoryBlock(const std::shared_ptr<MemoryBlock> block) override;
};

class PlatformCardFactory {
public:
  static std::shared_ptr<PlatformCard>
  make(json_t *json_card, std::string card_name,
       std::shared_ptr<kernel::vfio::Container> vc,
       const std::filesystem::path &searchPath);
};

} /* namespace fpga */
} /* namespace villas */
