{
  cmake,
  lib,
  libnice,
  libpcap,
  pkg-config,
  stdenv,
  src,
  openssl,
}:
stdenv.mkDerivation {
  pname = "libdatachannel";
  version = "villas";
  src = src;
  nativeBuildInputs = [cmake pkg-config];
  buildInputs = [libnice libpcap openssl];
  cmakeFlags = [
    "-DUSE_NICE=ON" # use libnice for better protocol support
    "-DNO_WEBSOCKET=ON" # villas uses libwebsockets instead
    "-DNO_MEDIA=ON" # villas does not use media transport features
  ];
  meta = with lib; {
    description = "C/C++ WebRTC network library featuring Data Channels, Media Transport, and WebSockets";
    homepage = "https://libdatachannel.org/";
    license = licenses.mpl20;
  };
}
