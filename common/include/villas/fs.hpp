/* std::filesystem compatibility wrapper
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/config.hpp>

#ifdef WITH_GHC_FS

#include <ghc/filesystem.hpp>
#include <fmt/ostream.h>

namespace fs = ghc::filesystem;

template <>
class fmt::formatter<fs::path> : public fmt::ostream_formatter {};

#else

#include <filesystem>
namespace fs = std::filesystem;

#endif
