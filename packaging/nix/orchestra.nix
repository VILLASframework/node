# SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

{
  version ? "2025.1.3",

  lib,
  stdenv,
  autoPatchelfHook,
  makeWrapper,
  runCommand,
  fetchurl,

  libredirect,
  nettools,
  libuuid,
  unzip,
  rpm,
  cpio,
  cacert,
  jq,
  curl,
  gnused,
}:
let
  inherit (lib)
    concatStringsSep
    escapeURL
    mapAttrsToList
    replaceStrings
    ;

  versionHashes = {
    "2025.1.2" = "sha256-XZp5lprMwBAst3LTIVYoOfUpk1p66EJ4mY/nYPdBfIE=";
    "2025.1.3" = "sha256-ISoRxPUHE1KpdvXfLyFujLixTtfcG6jRJoKgMwIwynY=";
  };

  blobName = "RT-LAB_${version}.zip";

  megainstaller-zip =
    runCommand blobName
      {
        outputHash = versionHashes.${version};
        outputHashAlgo = null;

        buildInputs = [
          cacert
          curl
          jq
          gnused
        ];
      }
      ''
        SAS=$(curl -s https://www.opal-rt.com/wp-json/wp/v2/pages/2058 | jq -r .content.rendered | sed -En 's|.*https://blob\.opal-rt\.com/softwares/[^?]+\?([^"]*).*|\1|p' | sed 's|&amp;|\&|g' | head -1)
        curl -o $out "https://blob.opal-rt.com/softwares/RT-LAB/${blobName}?$SAS"
      '';

  megainstaller = runCommand "rtlab-megainstaller-${version}" { inherit version; } ''
    ${unzip}/bin/unzip ${megainstaller-zip} -d $out
  '';

  target = runCommand "rtlab-target-${version}" { inherit version; } ''
    ${unzip}/bin/unzip ${megainstaller}/Files/RT-LAB/data/target.zip
    mv target $out
  '';

  target_rpm = runCommand "rtlab-target-${version}-rpm" { inherit version; } ''
    mkdir $out
    cd $out

    ${rpm}/bin/rpm2cpio ${target}/rt_linux64/rtlab-rt_linux64-*.rpm | ${cpio}/bin/cpio -idmv
  '';
in
stdenv.mkDerivation {
  pname = "libOpalOrchestra";
  inherit version;
  src = target_rpm;

  nativeBuildInputs = [
    autoPatchelfHook
    makeWrapper
  ];

  buildInputs = [
    libuuid
    stdenv.cc.cc.lib
  ];

  installPhase = ''
    mkdir -p $out/{lib,bin,include}

    cd usr/opalrt/v2025.1.3.77/common

    cp "bin/OrchestraExtCommIP" "$out/bin/"
    cp "include_target/RTAPI.h" "$out/include/"
    cp "bin/libsimulation-configuration.so" "$out/lib/"
    cp "bin/libOpalOrchestra.so" "$out/lib/libOpalOrchestra.so"
  '';

  preFixup = ''
    wrapProgram $out/bin/OrchestraExtCommIP \
      --set LD_PRELOAD "${libredirect}/lib/libredirect.so" \
      --set NIX_REDIRECTS "/sbin/ifconfig=${nettools}/bin/ifconfig" \
  '';
}
