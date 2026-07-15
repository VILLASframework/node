#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2014-2025 The VILLASframework Authors
# SPDX-License-Identifier: Apache-2.0
#
# Bundle the OpenAPI YAML schema and embed it as a C++ raw string literal.
#
# Usage: tools/bundle_schema.sh <output.cpp>

set -euo pipefail

TOPLEVEL="$(git rev-parse --show-toplevel)"

bundled_schema() {
    redocly bundle villas-node \
        --config "${TOPLEVEL}/doc/redocly.yaml" \
        --skip-preprocessor=villas/expand-discriminator \
        --dereferenced \
        --ext json | \
    jq --compact-output
}

BUNDLED_SCHEMA="$(bundled_schema)"

cat > "${TOPLEVEL}/lib/json_schema.cpp" <<<"
// SPDX-FileCopyrightText: 2014-2025 The VILLASframework Authors
// SPDX-License-Identifier: Apache-2.0
// Generated file — do not edit

#include <villas/json.hpp>

namespace villas::node {

Json const &bundled_schemas() {
  // clang-format off
  static auto const schema = Json::parse(R\"BUNDLED_SCHEMA(${BUNDLED_SCHEMA})BUNDLED_SCHEMA\");
  // clang-format on

  return schema;
}

} // namespace villas::node"
