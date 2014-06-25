# Readme \section Readme

This is the readme file for the S2SS server. Take a look at the `doc/html` directory for a full documentation.

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

## Usage

The S2SS server (`server`) expects the path to a configuration file as a single argument.

The server requires root privileges during the startup.
Afterwards privileges can be dropped by using the `user` and `group` settings in the config file.

	Usage: ./server CONFIG
	  CONFIG is a required path to a configuration file

	Simulator2Simulator Server 0.1-d7de19c (Jun  4 2014 02:50:13)
	 Copyright 2014, Institute for Automation of Complex Power Systems, EONERC
	   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>


### Examples

 1. Send/Receive of random data:

	$ ./random 1 4 100 | ./send 4 192.168.1.12:10200

 2. Ping/Pong Latency

	$ ./test latency 192.168.1.12:10200
