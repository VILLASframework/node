{
  cmake,
  lib,
  stdenv,
  src,
  openssl,
}:
stdenv.mkDerivation {
  pname = "libdatachannel";
  version = "villas";
  src = src;
  nativeBuildInputs = [cmake];
  buildInputs = [openssl];
  meta = with lib; {
    description = "C/C++ WebRTC network library featuring Data Channels, Media Transport, and WebSockets";
    homepage = "https://libdatachannel.org/";
    license = licenses.mpl20;
  };
}
