/* std::filesystem compatability
 *
 * SPDX-FileCopyrightText: 2025, ghc::filesystem Authors
 * SPDX-License-Identifier: MIT
 */

#include <villas/config.hpp>

#ifdef WITH_GHC_FS
    #include <ghc/filesystem.hpp>
    namespace fs = ghc::filesystem;
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif