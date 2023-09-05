# Integration Tests

<!--
SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
-->

## Run tests

```
$ BUILDDIR=/VILLASnode/build/ /VILLASnode/tools/integration-tests.sh
```

There are two options for the test script:

```
-v         Show full test output
-f FILTER  Filter test cases
```

Example:

```
$ BUILDDIR=/VILLASnode/build/ /VILLASnode/tools/integration-tests.sh -f pipe-loopback-socket -v
```
