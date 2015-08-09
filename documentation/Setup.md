# Setup

## Compilation

### Prerequisites

Install libraries including developement headers for:

 - [libconfig](http://www.hyperrealm.com/libconfig/) for parsing the config file
 - [libnl3](http://www.infradead.org/~tgr/libnl/) for the network emulation support
 
Use the following command to install the dependencies under Debian-based distributions:

    $ sudo apt-get install iproute2 libconfig-dev linbl-3-dev

or the following line for Fedora / CentOS / Redhat systems:

    $ sudo yum install iproute2 libconfig-devel libnl3-devel

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

