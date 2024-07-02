/* Platform Interrupt Controller
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/ips/intc.hpp>

class PlatformInterruptController
    : public villas::fpga::ip::InterruptController {
public:
  bool init() override;

  bool enableInterrupt(InterruptController::IrqMaskType mask,
                       bool polling) override;
  bool enableInterrupt(IrqPort irq, bool polling) override;
};
