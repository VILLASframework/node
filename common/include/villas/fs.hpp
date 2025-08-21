/* std::filesystem compatibility wrapper
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/config.hpp>

#ifdef WITH_GHC_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
