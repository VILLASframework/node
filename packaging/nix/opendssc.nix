# SPDX-FileCopyrightText: 2025 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  lib,
  stdenv,
  cmake,
  fetchsvn,
  libuuid,
}:
stdenv.mkDerivation {
  pname = "opendssc";
  version = "10.1.0.1";

  src = fetchsvn {
    url = "https://svn.code.sf.net/p/electricdss/code/trunk/VersionC";
    rev = 4020;
    hash = "sha256-9mCnjNZEQNsc75NbuUU0QHKf9bX9HsMsYKXuOJVQ+VE=";
  };

  nativeBuildInputs = [ cmake ];

  buildInputs = [ libuuid ];

  cmakeFlags = [ "-DMyOutputType=DLL" ];

  enableParallelBuilding = true;

  patchFlags = [ "--binary" "--strip=1" ]; # OpenDSS is using CRLF line endings
  patches = [
    ../patches/0001-opendssc-cmakelists.patch
    ../patches/0002-opendssc-set-data-path-chdir.patch
  ];

  postInstall = ''
    mv $out/openDSSC/bin/*.so $out/lib/
    rm -rf $out/openDSSC
  '';

  meta = {
    description = "EPRI Distribution System Simulator";
    homepage = "https://sourceforge.net/projects/electricdss/";
    license = lib.licenses.bsd3;
    maintainers = with lib.maintainers; [ stv0g ];
    platforms = lib.platforms.unix;
  };
}
