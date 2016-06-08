# Setup

## Compilation

### Prerequisites

Install libraries and developement headers for:

 - [libconfig](http://www.hyperrealm.com/libconfig/) for parsing the configuration file.
 - [libnl3](http://www.infradead.org/~tgr/libnl/) for the network communication & emulation support of the `socket` node-type.
 - libOpal{AsyncApi,Core,Utils} for running VILLASnode as an Asynchronous process inside your RT-LAB model.
 - [pciuitils](http://mj.ucw.cz/sw/pciutils/) for enumerating PCI devices. Required by `gtfpga` node-type.
 - [libjansson](http://www.digip.org/jansson/) JSON parser for `websocket` and `ngsi` node-types.
 - [libwebsockets](http://libwebsockets.org) for the `websocket` node-type.
 - [libcurl](https://curl.haxx.se/libcurl/) for HTTP REST requests by the `ngsi` node-type.
 - [libuuid](http://sourceforge.net/projects/libuuid/) for generating random IDs. Required by the `ngsi` node-type.
 
Use the following command to install the dependencies under Debian-based distributions:

    $ sudo apt-get install build-essential pkg-config libconfig-dev libnl-3-dev libnl-route-3-dev libpci-deb libjansson-dev libcurl4-openssl-dev uuid-dev

or the following line for Fedora / CentOS / Redhat systems:

    $ sudo yum install pkgconfig gcc make libconfig-devel libnl3-devel pciutils-devel libcurl-devel jansson-devel libuuid-devel

### Compilation

Checkout the `Makefile` and `include/config.h` for some options which have to be specified at compile time.

Afterwards, start the compilation with:

    $ make

Append `V=5` to `make` for a more verbose debugging output.
Append `DEBUG=1` to `make` to add debug symbols.

### Installation

Install the files to your search path:

    $ make install

Append `PREFIX=/opt/local` to change the installation destination.

### Test

Verify everything is working and required node-types are compiled-in:

    $ villas server

Will show you the current version of the server including a list of all supported node-types.
