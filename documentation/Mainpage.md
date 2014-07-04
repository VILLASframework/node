S2SS is a client server application based on UDP/IP to connect simulation equipment.

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

	Usage: ./server CONFIG
	  CONFIG is a required path to a configuration file

	Simulator2Simulator Server 0.1-d7de19c (Jun  4 2014 02:50:13)
	 Copyright 2014, Institute for Automation of Complex Power Systems, EONERC
	   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

The server requires root privileges for:

 - Enable the realtime fifo scheduler
 - Increase the task priority
 - Configure the network emulator (netem)
 - Change the SMP affinity of threads and network interrupts

### Examples

 1. Start server:

	$ ./server etc/example.conf

 2. Receive/dump data to file

	$ ./receive *:10200 > dump.csv

 3. Replay recorded data:

	$ ./send 4 192.168.1.12:10200 < dump.csv

 4. Send random generated values:

	$ ./random 1 4 100 | ./send 4 192.168.1.12:10200

 5. Test ping/pong latency:

	$ ./test latency 192.168.1.12:10200
