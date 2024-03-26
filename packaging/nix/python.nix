# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  src,
  pkgs,
  python3Packages,
  villas-minimal,
}:
python3Packages.buildPythonPackage {
  name = "villas-python";
  src = "${src}/python";
  format = "pyproject";
  propagatedBuildInputs = with python3Packages; [
    linuxfd
    requests
    protobuf
  ];
  build-system = with python3Packages; [
    setuptools
  ];
  nativeCheckInputs = with python3Packages; [
    black
    flake8
    mypy
    pytest
    types-requests
  ];

  postPatch = ''
    ${pkgs.protobuf}/bin/protoc --proto_path ${src}/lib/formats --python_out=villas/node/ ${src}/lib/formats/villas.proto
  '';
}
