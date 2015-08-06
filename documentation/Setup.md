# Server

@subpage configuration
@subpage usage
@subpage netem

## Compilation

### Prerequisites

Install libraries including developement headers for:

 - _libconfig_ for parsing the config file

	$ sudo apt-get install iproute2 libconfig-dev libsodium-dev
or:

	$ sudo yum install iproute2 libconfig-devel libsodium-devel

**Important:** Please note that the server requires the
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
tools to setup the network emulation and interfaces.

### Compilation

Checkout the `Makefile` and `include/config.h` for some config options which have to be specified by compile time.

Afterwards, start the compilation with:

	$ make

Add `V=5` for a more verbose debugging output.

## Installation

Install the server by executing:

	$ sudo make install

Add `PREFIX=/usr/local/` to specify a non-standard installation destination.

