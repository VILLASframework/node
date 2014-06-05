# Readme \section Readme

This is the readme file for the S2SS server. Take a look at the `doc/html` directory for a full documentation.

## Contact

This project is developed at the [Institute for Automation of Complex Power Systems](www.acs.eonerc.rwth-aachen.de) (ACS) at the EON Energy Research Center (EONERC) at the [RWTH University](http://www.rwth-aachen.de) in Aachen.

 - Steffen Vogel <StVogel@eonerc.rwth-aachen.de>
 - Marija Stevic <MStevic@eonerc.rwth-aachen.de>

## Compilation

Install libraries including developement headers for:

 - libconfig
 - libnl-3
 - libnl-route-3

Start the compilation with:

	$ make

Add `V=5` for a more verbose debugging output.

## Installation

Install the server by executing:

	$ make install

Add `PREFIX=/usr/local/` to specify a non-standard installation destination.

The s2ss server needs several [capabilities(7)](http://man7.org/linux/man-pages/man7/capabilities.7.html) to run:

 - `CAP_NET_ADMIN` to increase the socket priority
 - `CAP_NET_BIND_SERVICE` to listen to UDP ports below 1024
 - `CAP_SYS_NICE` to set the realtime priority and cpu affinity
 - `CAP_IPC_LOC` to lock pages for better realtime

## Configuration

See [configuration](Configuration.md) for more information.

## Usage

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

## A Operating System and Kernel

Kernel command line: isolcpus=[cpu_number]

Map NIC IRQs	=> ???
Map Tasks	=> taskset or sched_cpuaffinity
Nice Task	=> Realtime Priority

Linux RT-preemp: https://rt.wiki.kernel.org/index.php/Main_Page
Precompiled kernels: http://ccrma.stanford.edu/planetccrma/software/
			for Fedora 20 (https://fedoraproject.org/)

