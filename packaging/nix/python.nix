# SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0
{
  src,
  python3Packages,
  villas-minimal,
}:
python3Packages.buildPythonPackage {
  name = "villas-python";
  src = src;
  propagatedBuildInputs = with python3Packages; [
    linuxfd
    requests
    villas-minimal
  ];
  nativeCheckInputs = with python3Packages; [
    black
    flake8
    mypy
    pytest
    types-requests
  ];
}
