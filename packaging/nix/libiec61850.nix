# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  cmake,
  lib,
  stdenv,
  src,
}:
stdenv.mkDerivation {
  pname = "libiec61850";
  version = "villas";
  src = src;
  separateDebugInfo = true;
  nativeBuildInputs = [cmake];
  meta = with lib; {
    description = "open-source library for the IEC 61850 protocols";
    homepage = "https://libiec61850.com/";
    license = licenses.gpl3;
  };
}
