# Setup

## Compilation

### Prerequisites

Install libraries including developement headers for:

 - [libconfig](http://www.hyperrealm.com/libconfig/) for parsing the config file
 - [libnl3](http://www.infradead.org/~tgr/libnl/) for the network communication & emulation support
 - libOpal{AsyncApi,Core,Utils} for running the S2SS server as an Asynchronous process inside your RT-LAB model 
 
Use the following command to install the dependencies under Debian-based distributions:

    $ sudo apt-get install build-essential pkg-config libconfig-dev libnl-3-dev libnl-route-3-dev libpci-deb libjansson-dev libcurl4-openssl-dev uuid-dev

or the following line for Fedora / CentOS / Redhat systems:

    $ sudo yum install pkgconfig gcc make libconfig-devel libnl3-devel pciutils-devel libcurl-devel jansson-devel libuuid-devel

**Important:** Please note that the server requires the
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
tools to setup the network emulation and interfaces.

### Compilation

Checkout the `Makefile` and `include/config.h` for some options which have to be specified at compile time.

Afterwards, start the compilation with:

	$ make

Append `V=5` to `make` for a more verbose debugging output.

Append `DEBUG=1` to `make` to add debug symbols.
