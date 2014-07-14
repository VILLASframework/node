S2SS is a client server application based on UDP/IP to connect simulation equipment.

It's designed with focus on low latency to establish soft-realtime communication.

## Contact

This project is developed at the [Institute for Automation of Complex Power Systems](www.acs.eonerc.rwth-aachen.de) (ACS) at the EON Energy Research Center (EONERC) at the [RWTH University](http://www.rwth-aachen.de) in Aachen.

 - Steffen Vogel <StVogel@eonerc.rwth-aachen.de>
 - Marija Stevic <MStevic@eonerc.rwth-aachen.de>

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


