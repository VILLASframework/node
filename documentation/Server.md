# Server

@subpage configuration
@subpage usage
@subpage netem

## Compilation

Install libraries including developement headers for:

 - libconfig

Start the compilation with:

	$ make

Add `V=5` for a more verbose debugging output.

## Installation

Install the server by executing:

	$ sudo make install

Add `PREFIX=/usr/local/` to specify a non-standard installation destination.

**Important:** Please note that the server requires the
[iproute2](http://www.linuxfoundation.org/collaborate/workgroups/networking/iproute2)
tools to setup the network emulation and interfaces.

Install these via:

	$ sudo yum install iproute2
or:

	$ sudo apt-get install iproute2

## Configuration

See [configuration](Configuration.md) for more information.

@todo Move documentation from Mainpage to Server.
