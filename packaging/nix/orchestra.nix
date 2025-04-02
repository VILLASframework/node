# SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  libredirect,
  nettools,
  fetchurl,
  autoPatchelfHook,
  dpkg,
  libuuid,
  makeWrapper ,
}:
let
  # src = requireFile {
  #   name = "componentorchestra_7.6.2_amd64.deb";
  #   hash = "sha256-2cQtYkf1InKrDPL5UDQDHaYM7bq21Dw77itfFwuXa54=";
  # };

  src = fetchurl {
    url = "https://blob.opal-rt.com/softwares/rt-lab-archives/componentorchestra_7.6.2_amd64.deb?sp=r&st=2024-10-30T06:31:59Z&se=2034-11-30T14:31:59Z&spr=https&sv=2022-11-02&sr=b&sig=cnKY8RxZf8hv91gWLIBG6iBGSVziXkKR3%2BOYIE6MSkI%3D";
    hash = "sha256-2cQtYkf1InKrDPL5UDQDHaYM7bq21Dw77itfFwuXa54=";
  };
in
stdenv.mkDerivation {
  pname = "libOpalOrchestra";
  version = "7.6.2";
  inherit src;

  dontUnpack = true;

  nativeBuildInputs = [
    dpkg
    autoPatchelfHook
    makeWrapper
  ];

  buildInputs = [
    libuuid
    stdenv.cc.cc.lib
  ];

  installPhase = ''
    ${dpkg}/bin/dpkg-deb -x ${src} .

    mv usr/opalrt/exportedOrchestra $out
    mv $out/bin/OrchestraExtCommIPDebian $out/bin/OrchestraExtCommIP
  '';

  preFixup = ''
    wrapProgram $out/bin/OrchestraExtCommIP \
      --set LD_PRELOAD "${libredirect}/lib/libredirect.so" \
      --set NIX_REDIRECTS "/sbin/ifconfig=${nettools}/bin/ifconfig" \
  '';
}
