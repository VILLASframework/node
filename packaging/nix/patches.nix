# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University#
# SPDX-License-Identifier: Apache-2.0
#
# This overlay contains patches to dependencies of villas-node.
# It is only guaranteed to work for the locked version of nixpkgs,
# future updates to upstream nixpkgs may make these obsolete.
final: prev: {
  nlohmann_json_schema_validator = prev.nlohmann_json_schema_validator.overrideAttrs (prevAttrs: {
    patches = (prevAttrs.patches or []) ++ [
      # fix broken default values patch: https://github.com/pboettch/json-schema-validator/pull/370
      (final.fetchpatch {
        url = "https://github.com/pboettch/json-schema-validator/commit/0443130712a15df239811e0827966c50547959cf.patch";
        hash = "sha256-AK0l0hfZU1bBG5/mDGw1gyN9xxLu03L+xFaQp95reX4=";
      })

      # report all diagnostics of additionalProperties: https://github.com/pboettch/json-schema-validator/pull/383
      (final.fetchpatch {
        url = "https://github.com/pboettch/json-schema-validator/commit/0a061738a23e101cc3f1e40e31c745fe8b1f669c.patch";
        hash = "sha256-3Pmr03ptiGD9xR4ryBaARzOsvPTY85lhaf0PVFEtTww=";
      })

      # fix default values for condition directives: https://github.com/pboettch/json-schema-validator/pull/383
      (final.fetchpatch {
        url = "https://github.com/pboettch/json-schema-validator/commit/1cbf7099eb2a1cb5c46509ed331767ba23bdd3d6.patch";
        hash = "sha256-GxJBqeKCcd+9Mw3lsH3+vDWbgu9bgwQTgVOtqMQceS0=";
      })

      # better error messages for unknown properties/items: https://github.com/pboettch/json-schema-validator/pull/383
      (final.fetchpatch {
        url = "https://github.com/pboettch/json-schema-validator/commit/66f0a7fbb9478713ce1b73563d2f81fcd29692f0.patch";
        hash = "sha256-J6+uLgq+b1SFNK0k+A0VmyC7v9XPI34lefLe9mgohac=";
      })
    ];
  });
}
