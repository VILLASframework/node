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
  # Format-types
  withFormatProtobuf ? withAllFormats,
  # Hook-types
  withHookLua ? withAllHooks,
  # Node-types
  withNodeAmqp ? withAllNodes,
  withNodeComedi ? withAllNodes,
  withNodeFpga ? withAllNodes,
  withNodeEthercat ? withAllNodes,
  withNodeIec60870 ? withAllNodes,
  withNodeIec61850 ? withAllNodes,
  withNodeInfiniband ? withAllNodes,
  withNodeKafka ? withAllNodes,
  withNodeModbus ? withAllNodes,
  withNodeMqtt ? withAllNodes,
  withNodeNanomsg ? withAllNodes,
  withNodeRedis ? withAllNodes,
  withNodeRtp ? withAllNodes,
  withNodeSocket ? withAllNodes,
  withNodeTemper ? withAllNodes,
  withNodeUldaq ? withAllNodes,
  withNodeWebrtc ? withAllNodes,
  withNodeZeromq ? withAllNodes,
  # Minimal dependencies
  cmake,
  common,
  coreutils,
  fpga,
  graphviz,
  jq,
  lib,
  makeWrapper,
  pkg-config,
  stdenv,
  # Optional dependencies
  comedilib,
  curl,
  czmq,
  ethercat,
  gnugrep,
  jansson,
  lib60870,
  libconfig,
  libdatachannel,
  libiec61850,
  libmodbus,
  libnl,
  libre,
  libsodium,
  libuldaq,
  libusb,
  libuuid,
  libwebsockets,
  lua,
  mosquitto,
  nanomsg,
  openssl,
  pkgsBuildBuild,
  protobufc,
  protobufcBuildBuild ? pkgsBuildBuild.protobufc,
  rabbitmq-c,
  rdkafka,
  rdma-core,
  redis-plus-plus,
  spdlog,
}:
stdenv.mkDerivation {
  inherit src version;
  pname = "villas";
  outputs = ["out" "dev"];
  separateDebugInfo = true;
  cmakeFlags =
    []
    ++ lib.optionals (!withGpl) ["-DWITHOUT_GPL=ON"]
    ++ lib.optionals withFormatProtobuf ["-DCMAKE_FIND_ROOT_PATH=${protobufcBuildBuild}/bin"];
  postPatch = ''
    patchShebangs --host ./tools
  '';
  preConfigure = ''
    rm -df common
    rm -df fpga
    ln -s ${common} common
    ${lib.optionalString withNodeFpga "ln -s ${fpga} fpga"}
  '';
  postInstall = ''
    if [ -d $out/include/villas/ ] && [ -d $dev/include/villas/ ]; then
      mv $out/include/villas/* $dev/include/villas/
      rm -d $out/include/villas
    fi
    wrapProgram $out/bin/villas \
      --set PATH ${lib.makeBinPath [(placeholder "out") gnugrep coreutils]}
    wrapProgram $out/bin/villas-api \
      --set PATH ${lib.makeBinPath [coreutils curl jq]}
  '';
  nativeBuildInputs = [
    cmake
    makeWrapper
    pkg-config
  ];
  depsBuildBuild = lib.optionals withFormatProtobuf [protobufcBuildBuild];
  buildInputs =
    [
      jansson
      libwebsockets
      libuuid
      openssl
      curl
      spdlog
    ]
    ++ lib.optionals withExtraConfig [libconfig]
    ++ lib.optionals withExtraGraphviz [graphviz]
    ++ lib.optionals withFormatProtobuf [protobufc]
    ++ lib.optionals withHookLua [lua]
    ++ lib.optionals withNodeAmqp [rabbitmq-c]
    ++ lib.optionals withNodeComedi [comedilib]
    ++ lib.optionals withNodeEthercat [ethercat]
    ++ lib.optionals withNodeIec60870 [lib60870]
    ++ lib.optionals withNodeIec61850 [libiec61850]
    ++ lib.optionals withNodeInfiniband [rdma-core]
    ++ lib.optionals withNodeKafka [rdkafka]
    ++ lib.optionals withNodeModbus [libmodbus]
    ++ lib.optionals withNodeMqtt [mosquitto]
    ++ lib.optionals withNodeNanomsg [nanomsg]
    ++ lib.optionals withNodeRedis [redis-plus-plus]
    ++ lib.optionals withNodeRtp [libre]
    ++ lib.optionals withNodeSocket [libnl]
    ++ lib.optionals withNodeTemper [libusb]
    ++ lib.optionals withNodeUldaq [libuldaq]
    ++ lib.optionals withNodeWebrtc [libdatachannel]
    ++ lib.optionals withNodeZeromq [czmq libsodium];
  meta = with lib; {
    mainProgram = "villas";
    description = "a tool connecting real-time power grid simulation equipment";
    homepage = "https://villas.fein-aachen.org/";
    license = licenses.asl20;
  };
}
