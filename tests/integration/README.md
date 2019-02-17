# Integration Tests

Run tests:

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
