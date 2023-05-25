{
  cmake,
  lib,
  stdenv,
  src
}:
stdenv.mkDerivation {
  pname = "libiec61850";
  version = "villas";
  src = src;
  nativeBuildInputs = [cmake];
  meta = with lib; {
    description = "open-source library for the IEC 61850 protocols";
    homepage = "https://libiec61850.com/";
    license = licenses.gpl3;
  };
}
