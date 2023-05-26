{
  # build dependencies
  cmake,
  common,
  lib,
  makeWrapper,
  pkg-config,
  stdenv,
  src,
  # configuration
  withConfig ? false,
  withProtobuf ? false,
  withAllNodes ? false,
  withNodeComedi ? withAllNodes,
  withNodeIec60870 ? withAllNodes,
  withNodeIec61850 ? withAllNodes,
  withNodeInfiniband ? withAllNodes,
  withNodeKafka ? withAllNodes,
  withNodeMqtt ? withAllNodes,
  withNodeRedis ? withAllNodes,
  withNodeUldaq ? withAllNodes,
  withNodeZeromq ? withAllNodes,
  withNodeNanomsg ? withAllNodes,
  withNodeRtp ? withAllNodes,
  withNodeTemper ? withAllNodes,
  withNodeSocket ? withAllNodes,
  # dependencies
  comedilib,
  coreutils,
  curl,
  czmq,
  gnugrep,
  graphviz,
  jansson,
  lib60870,
  libconfig,
  libiec61850,
  libre,
  libnl,
  libsodium,
  libuldaq,
  libusb,
  libuuid,
  libwebsockets,
  mosquitto,
  nanomsg,
  openssl,
  protobuf,
  protobufc,
  rdkafka,
  rdma-core,
  spdlog,
}:
stdenv.mkDerivation {
  pname = "villas";
  version = "release";
  src = src;
  cmakeFlags = [
    "-DWITH_FPGA=OFF"
    "-DDOWNLOAD_GO=OFF"
    "-DCMAKE_BUILD_TYPE=Release"
    # the default -O3 causes g++ warning false positives on
    # 'array-bounds' and 'stringop-overflow' for villas-relay
    "-DCMAKE_CXX_FLAGS_RELEASE=-Wno-error"
  ];
  preConfigure = ''
    rm -d common && ln -sf ${common} common
  '';
  postInstall = ''
    wrapProgram $out/bin/villas \
      --set PATH ${lib.makeBinPath [(placeholder "out") gnugrep coreutils]}
  '';
  nativeBuildInputs = [
    cmake
    makeWrapper
    pkg-config
  ];
  buildInputs =
    [
      jansson
      libwebsockets
      libuuid
      openssl
      curl
      spdlog
    ]
    ++ lib.optionals withConfig [libconfig]
    ++ lib.optionals withProtobuf [protobuf protobufc]
    ++ lib.optionals withNodeComedi [comedilib]
    ++ lib.optionals withNodeZeromq [czmq libsodium]
    ++ lib.optionals withNodeIec60870 [lib60870]
    ++ lib.optionals withNodeIec61850 [libiec61850]
    ++ lib.optionals withNodeSocket [libnl]
    ++ lib.optionals withNodeRtp [libre]
    ++ lib.optionals withNodeUldaq [libuldaq]
    ++ lib.optionals withNodeTemper [libusb]
    ++ lib.optionals withNodeMqtt [mosquitto]
    ++ lib.optionals withNodeNanomsg [nanomsg]
    ++ lib.optionals withNodeKafka [rdkafka]
    ++ lib.optionals withNodeInfiniband [rdma-core];
  meta = with lib; {
    description = "a tool connecting real-time power grid simulation equipment";
    homepage = "https://villas.fein-aachen.org/";
    license = licenses.asl20;
  };
}
