/* Factory for platform cards
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <villas/fpga/card/platform_card.hpp>
#include <villas/kernel/vfio_container.hpp>

struct json_t;
using namespace villas;

namespace villas {
namespace fpga {

class PlatformCardFactory {
public:
  static std::shared_ptr<PlatformCard>
  make(json_t *json_card, std::string card_name,
       std::shared_ptr<kernel::vfio::Container> vc,
       const std::filesystem::path &searchPath);
};

} // namespace fpga
} // namespace villas
