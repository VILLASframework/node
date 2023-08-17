{
  autoconf,
  automake,
  cmake,
  lib,
  libtool,
  pkg-config,
  stdenv,
  src,
}:
stdenv.mkDerivation {
  pname = "ethercat";
  version = "villas";
  src = src;
  separateDebugInfo = true;
  nativeBuildInputs = [autoconf automake libtool pkg-config];
  preConfigure = ''
    bash ./bootstrap
  '';
  configureFlags = [
    "--enable-userlib=yes"
    "--enable-kernel=no"
  ];
  meta = with lib; {
    description = "IgH EtherCAT Master for Linux";
    homepage = "https://etherlab.org/master";
    license = licenses.gpl2Only;
  };
}
