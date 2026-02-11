# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  # General configuration
  src,
  version,
  withGpl ? true,
  withAllExtras ? false,
  withAllFormats ? false,
  withAllHooks ? false,
  withAllNodes ? false,
  # Extra features
  withExtraConfig ? withAllExtras,
  withExtraGraphviz ? withAllExtras,
  withExtraTesting ? (withAllExtras && system == "x86_64-linux"),
  # Format-types
  withFormatProtobuf ? withAllFormats,
  # Hook-types
  withHookLua ? withAllHooks,
  # Node-types
  withNodeAmqp ? withAllNodes,
  withNodeComedi ? withAllNodes,
  withNodeEthercat ? (withAllNodes && system == "x86_64-linux"),
  withNodeIec60870 ? withAllNodes,
  withNodeIec61850 ? withAllNodes,
  withNodeInfiniband ? withAllNodes,
  withNodeKafka ? withAllNodes,
  withNodeModbus ? withAllNodes,
  withNodeMqtt ? withAllNodes,
  withNodeNanomsg ? withAllNodes,
  withNodeOpenDSS ? withAllNodes,
  withNodeOpalOrchestra ? (withAllNodes && system == "x86_64-linux"),
  withNodeRedis ? withAllNodes,
  withNodeRtp ? withAllNodes,
  withNodeSocket ? withAllNodes,
  withNodeTemper ? withAllNodes,
  withNodeUldaq ? withAllNodes,
  withNodeWebrtc ? withAllNodes,
  withNodeZeromq ? withAllNodes,
  # Minimal dependencies
  bash,
  cmake,
  coreutils,
  graphviz,
  jq,
  lib,
  makeWrapper,
  pkg-config,
  stdenv,
  system,
  nlohmann_json,
  bzip2,
  # Optional dependencies
  boxfort,
  comedilib,
  criterion,
  curl,
  czmq,
  cyrus_sasl,
  ethercat,
  gnugrep,
  jansson,
  lib60870,
  libconfig,
  libdatachannel,
  libffi,
  libiec61850,
  libgit2,
  libmodbus,
  libnl,
  libre,
  libsodium,
  libuldaq,
  libusb1,
  libuuid,
  libwebsockets,
  libxml2,
  lua,
  mosquitto,
  nanomsg,
  opendssc,
  openssl,
  orchestra,
  pcre2,
  pkgsBuildBuild,
  protobuf,
  protobufBuildBuild ? pkgsBuildBuild.protobuf,
  protobufc,
  protobufcBuildBuild ? pkgsBuildBuild.protobufc,
  rabbitmq-c,
  rdkafka,
  rdma-core,
  redis-plus-plus,
  spdlog,
  linuxHeaders,
}:
stdenv.mkDerivation {
  inherit src version;
  pname = "villas";
  outputs = [
    "out"
    "dev"
  ];
  enableParallelBuilding = true;
  separateDebugInfo = true;
  cmakeFlags =
    [ ]
    ++ lib.optionals (!withGpl) [ "-DWITHOUT_GPL=ON" ]
    ++ lib.optionals withFormatProtobuf [ "-DCMAKE_FIND_ROOT_PATH=${protobufBuildBuild}/bin" ];
  postPatch = ''
    patchShebangs --host ./tools
  '';

  postInstall = ''
    if [ -d $out/include/villas/ ] && [ -d $dev/include/villas/ ]; then
      mv $out/include/villas/* $dev/include/villas/
      rm -d $out/include/villas
    fi
    patchShebangs --host $out/bin/villas
    patchShebangs --host $out/bin/villas-api
    wrapProgram $out/bin/villas \
      --set PATH ${
        lib.makeBinPath [
          (placeholder "out")
          gnugrep
          coreutils
        ]
      }
    wrapProgram $out/bin/villas-api \
      --set PATH ${
        lib.makeBinPath [
          coreutils
          curl
          jq
        ]
      }
  '';

  nativeBuildInputs = [
    cmake
    makeWrapper
    pkg-config
  ];

  depsBuildBuild = lib.optionals withFormatProtobuf [
    protobufBuildBuild
    protobufcBuildBuild
  ];

  buildInputs = [
    libwebsockets
    openssl
    curl
    spdlog
    bash
    nlohmann_json
    bzip2
  ]
  ++ lib.optionals withExtraTesting [
    boxfort
    criterion
    libffi
    libgit2
    pcre2
  ]
  ++ lib.optionals withExtraGraphviz [ graphviz ]
  ++ lib.optionals withHookLua [ lua ]
  ++ lib.optionals withNodeAmqp [ rabbitmq-c ]
  ++ lib.optionals withNodeComedi [ comedilib ]
  ++ lib.optionals withNodeEthercat [ ethercat ]
  ++ lib.optionals withNodeIec60870 [ lib60870 ]
  ++ lib.optionals withNodeIec61850 [ libiec61850 ]
  ++ lib.optionals withNodeKafka [
    rdkafka
    cyrus_sasl
  ]
  ++ lib.optionals withNodeModbus [ libmodbus ]
  ++ lib.optionals withNodeMqtt [ mosquitto ]
  ++ lib.optionals withNodeNanomsg [ nanomsg ]
  ++ lib.optionals withNodeOpenDSS [ opendssc ]
  ++ lib.optionals withNodeOpalOrchestra [
    orchestra
    libxml2
  ]
  ++ lib.optionals withNodeRedis [ redis-plus-plus ]
  ++ lib.optionals withNodeRtp [ libre ]
  ++ lib.optionals withNodeSocket [ libnl ]
  ++ lib.optionals withNodeTemper [ libusb1 ]
  ++ lib.optionals withNodeUldaq [ libuldaq ]
  ++ lib.optionals withNodeWebrtc [ libdatachannel ]
  ++ lib.optionals withNodeZeromq [
    czmq
    libsodium
  ];

  propagatedBuildInputs = [
    libuuid
    jansson
  ]
  ++ lib.optionals withFormatProtobuf [
    protobuf
    protobufc
  ]
  ++ lib.optionals withNodeInfiniband [ rdma-core ]
  ++ lib.optionals withExtraConfig [ libconfig ];

  preFixup = lib.optional withNodeOpalOrchestra ''
    wrapProgram $out/bin/villas-node \
      --prefix PATH : ${orchestra}/bin/
  '';

  meta = {
    mainProgram = "villas";
    description = "a tool connecting real-time power grid simulation equipment";
    homepage = "https://villas.fein-aachen.org/";
    license = lib.licenses.asl20;
    maintainers = with lib.maintainers; [ stv0g ];
  };
}
