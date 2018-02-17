# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [0.6.1] - 2018-02-17

### Added

- New node-types:
  - `mqtt` for MQTT / Mosquitto
- `make help` target

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

- User documentation has been moved to separate repository: http://git.rwth-aachen.de/VILLASframework/Documentation

## [0.2.0] - 2015-09-22

### Added

- OPAL-RT RT-LAB example demonstrating usage of AsyncIP
- Scripts and configuration files for creating LiveUSB Fedora images
- Travis-CI configuration
- More user documentation

## [0.1.0] - 2014-06-10

### Added

- Doxygen is now used to build documentation
