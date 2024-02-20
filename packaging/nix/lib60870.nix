# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  cmake,
  lib,
  stdenv,
  src,
}:
stdenv.mkDerivation {
  pname = "lib60870";
  version = "villas";
  src = src;
  separateDebugInfo = true;
  nativeBuildInputs = [cmake];
  preConfigure = "cd lib60870-C";
  meta = with lib; {
    description = "implementation of the IEC 60870-5-101/104 protocol";
    homepage = "https://libiec61850.com/";
    license = licenses.gpl3;
  };
}
