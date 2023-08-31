# Changelog

<!--
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
-->

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [0.7.1] - 2019-01-23

### Fixed

 - Removed most of the static storage variables from libvillas and libvillas-common
   which caused crashes with DPsim.
 - Fixed compilation with Ubuntu 16.04 and GCC 7.2.
 - Fixed broken `shmem` node-type since addition of signal defintions.

### Changed

 - Node-types can now handle more than a single file-descriptor for poll() multiplexing.
 - Enable network emulation sub-system also for other node-types than `socket`.
   The `rtp` node-type will support it now as well.
 - Improve readabilty of log output

### Added

 - Initial rate-limiting support for `rtp` node-type based on
   Additive Increase / Multiplicative Decrease (AIMD) and RTCP.

## [0.7.0] - 2019-01-13

### Added

- A new sub-command `villas-relay` implements a client-to-client WebSocket relay.
  It can be used as a proxy for nodes which sit behind a NAT firewall.
- New node-types: `infiniband`, `rtp`, `uldaq`
- VILLASnode got a similarily named Python package: `villas.node`.
  It can be used to interact or start an VILLASnode instance. 

### Changed

- The syntax of the configuration file has changed in several places.
  Most node configs are now separated into `in` and `out` sections.
- We ported major parts of the VILLASnode code base to C++.
- Some of the common utilities have been moved to a new repo.
  VILLAScommon is used also by other related VILLAS components.
  Please make sure to checkout / update the `common/` Git sub-module.
- We build and test VILLASnode now with Fedora 29.

### Fixed

- All unit tests are running again. Most of the integration tests run again as well.
- We added support for the ARM architecture.

## [0.6.4] - 2018-07-18

### Added

- New client User Code Model (UCM) for OPAL-RT HYPERSIM digital real-time simulator
- New node-types:
  - `infiniband` is using the Infiniband IBverbs and RDMA CM APIs for a low latency interface between simualation nodes.
  - `comedi` adds support for ADC and DAC cards supported by the [Comedi Linux control and measurment device interface](http://comedi.org).
- All header files can now be imported into C++ code. Missing `extern "C"` declarations have been added.

### Changed

- The VILLASnode project is now built with [CMake](http://cmake.org). The old Makefile have been removed.
- The VILLASnode project will be gradually ported to C++. The tools (`/tools/`), executables (`/src/`) and plugins (`/plugins/`) have already been convert to C++ code.

## [0.6.3] - 2018-06-04

### Added

- The `csv` IO format has been splitted into a similar `tsv` IO format which uses tabulators instead of commas.
- The `struct queue_signalled` supports now synchronization under Mac OS X.
- A new `poll` setting allows the user to enable/disable the usage of `poll(2)` when reading from multiple source nodes.

### Changed

- Started splitting the node configuration into a send and receive side. This changes will be soon also reflected in a changed configuration file syntax.
- Docker images are now based on Fedora 28.

### Fixed

- We now correctly determine with terminal size when executed in GDB or by the CI runner.

## [0.6.2] - 2018-05-14

### Added

- A Docker application image can now be build in a single step using `make docker`. 

### Changed

- The IO format names have changed. They now use dots (`raw.flt32`) instead of hyphens (`raw-flt32`) in their name. Please update your configuration files accordingly.
- The configuration of many node-types is now splitted into seperate `in` and `out` sections. Please update your configuration files accordingly.

## [0.6.1] - 2018-02-17

### Changed

- Rewrite of IO formatting subsystem/

### Added

- New node-types:
  - `mqtt` for MQTT / Mosquitto
- A new `make help` target shows the current build configuration and available targets.

### Fixed

- Websocket node can now receive data from a "catch-all" connection and associate to the correct simulator

## [0.6.0] - 2017-12-20

### Added

- New node-types:
  - `iec61850-9-2` for IEC 61850 Sampled Values
  - `amqp` for AMQP / RabbitMQ
- New IO formats:
  - Google `protobuf`
- Added support for Unix Domain Sockets to `socket` node-type
- Python example client using new Protobuf and UDP/Unix sockets
- Built-in hooks can be disabled now
- Network emulation has been improved
  - can now load a delay distribution from the config instead from a separate file
  - can emulate delay correlation
  
### Removed

- Moved VILLASfpga related code into external library
  - `libxil` dependency is dropped

### Changed

- Packaging of Docker and RPM has been improved
- Upgraded Docker build containers to Fedora 27
- Updated submodules for most dependencies
- Use "LABEL" instead of "MAINTAINER" keyword in Dockerfiles

### Fixed

- Socket node-type supports now arbirarily sized packets
- Sample data format conversion for RAW IO formats
- Network emulation support is working again

## [0.5.1] - 2017-10-23

### Changed

- OPAL-RT AsyncIP client is only build if libopal submodule is present

### Fixed

- Build warnings and errors on Ubuntu 16.04

## [0.5.0] - 2017-10-18

### Added

- Changelog
- InfluxDB node-type
- Support for "add" / "any" path register modes

### Fixed

- Netem support is working again

### Changed

- `villas-pipe` now automatically generates correct seqeunce numbers

## [0.4.5] - 2017-09-24

### Fixed

- RPM packaging to include debug symbols in `-debuginfo` package

## [0.4.4] - 2017-09-24

### Fixed

- Invalid sequence numbers due to broken workaround

## [0.4.3] - 2017-09-24

### Changed

- Do not abort execution of socket node fails with `sendto: invalid argument`

### Fixed

- Several fixes for the mux / demux re-mapping feature

## [0.4.2] - 2017-09-19

### Fixed

- Improved stability of HTTP API

## [0.4.1] - 2017-09-16

## [0.4.0] - 2017-09-16

### Added

- API action to request status of node
- Javascript configuration example
- Pluggable IO format subsystem with plugins for:
  - `csv`: Comma-separated values
  - `json`: Javascript Object Notation
  - `raw`: 8/16/32/64 but binary floating point and integer data
  - `villas-human`: VILLAS human-readable format
  - `villas-binary`: VILLAS binary wire protocol

## [0.3.5] - 2017-08-10

### Added

- API action to restart node with new configuration
- API action to terminate node

### Fixed

- VILLASnode compiles now on Ubuntu 16.04

## [0.3.4] - 2017-07-28

### Fixed

- Several compiler warnings on CLang

## [0.3.3] - 2017-07-14

### Changed

- Paths do not require output nodes anymore

### Added

- Loopback node-type

### Fixed

- OPAL-RT AsyncIP model for RT-LAB 11.1.x

## [0.3.2] - 2017-07-07

### Added

- Signal generator node-type
- Valgrind test cases

## [0.3.1] - 2017-06-29

### Added

- ZeroMQ node-type
- nanomsg node-type
- Multicast support for socket node-type
- RTDS GTNET-SKT examples

## [0.3.0] - 2017-05-08


Major changes in all parts of the software

### Added

- Pluggable node-types with plugins for:
  - `file`: File / Log recording & replay
  - `fpga`: VILLASfpga interface to Xilinx VC707 FPGA board via PCIexpress
  - `ngsi`: FI-WARE NGSI-10 Open RESTful API for Orion Context Broker
  - `websocket`: WebSocket support for VILLASweb
  - `socket`: BSD network sockets
  - `shmem`: POSIX shared memory
- Web-mockup for demonstration of the `websocket` node-type

### Removed

- User documentation has been moved to separate repository: http://git.rwth-aachen.de/acs/public/villas/documentation

## [0.2.0] - 2015-09-22

### Added

- OPAL-RT RT-LAB example demonstrating usage of AsyncIP
- Scripts and configuration files for creating LiveUSB Fedora images
- Travis-CI configuration
- More user documentation

## [0.1.0] - 2014-06-10

### Added

- Doxygen is now used to build documentation
