# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University#
# SPDX-License-Identifier: Apache-2.0
#
# This overlay contains patches to dependencies of villas-node.
# It is only guaranteed to work for the locked version of nixpkgs,
# future updates to upstream nixpkgs may make these obsolete.
final: prev:
let
  inherit (final) lib;
in
{
  libiec61850 = prev.libiec61850.overrideAttrs {
    patches = [ ./libiec61850_debug_r_session.patch ];
    cmakeFlags = (prev.cmakeFlags or [ ]) ++ [
      "-DCONFIG_USE_EXTERNAL_MBEDTLS_DYNLIB=ON"
      "-DCONFIG_EXTERNAL_MBEDTLS_DYNLIB_PATH=${final.mbedtls}/lib"
      "-DCONFIG_EXTERNAL_MBEDTLS_INCLUDE_PATH=${final.mbedtls}/include"
    ];
    nativeBuildInputs = (prev.nativeBuildInputs or [ ]) ++ [ final.buildPackages.cmake ];
    buildInputs = [ final.mbedtls ];
    separateDebugInfo = true;
  };
}
