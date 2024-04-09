# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  src,
  pkgs,
  python3Packages,
}:
python3Packages.buildPythonPackage {
  name = "villas-node";
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
    types-protobuf
    mypy-protobuf

    pytestCheckHook
  ];

  postPatch = ''
    ${pkgs.protobuf}/bin/protoc --proto_path ${src}/lib/formats --mypy_out=villas/node --python_out=villas/node/ ${src}/lib/formats/villas.proto
  '';
}
