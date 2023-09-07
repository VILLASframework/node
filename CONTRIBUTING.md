# Contribution guidelines

<!--
SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
-->

## Coding standards

We are following the [LLVM C++ Coding Standards](https://llvm.org/docs/CodingStandards.html).

## Always work on feature branches

Please branch from `master` to create a new _feature_ branch.

Please create a new _feature_ branch for every new feature or fix.

## Do not commit directly to `master`.

Use your _feature_ branch.

Please rebase your work against the `develop` before submitting a merge reqeuest.

## Make the CI happy :-)

Only branches which pass the CI can be merged.
