# VILLASnode

[![build status](https://git.rwth-aachen.de/VILLASframework/VILLASnode/badges/develop/build.svg)](https://git.rwth-aachen.de/acs/VILLASnode/commits/develop)
[![coverage report](https://git.rwth-aachen.de/VILLASframework/VILLASnode/badges/develop/coverage.svg)](https://git.rwth-aachen.de/acs/VILLASnode/commits/develop)

This is VILLASnode, a gateway for processing and forwardning simulation data between real-time simulators.
VILLASnode is a client/server application to connect simulation equipment and software such as:

 - OPAL-RT eMegaSim,
 - RTDS GTFPGA cards,
 - Simulink,
 - LabView,
 - and FPGA models.

It's designed with a focus on very low latency to achieve almost realtime exchange of simulation data.
VILLASnode is used in distributed- and co-simulation scenarios and developed for the field of power grid simulation at the EON Energy Research Center in Aachen, Germany.

## Overview

The project consists of a server daemon and several client modules which are documented here.

[TOC]

### Server

The server simply acts as a gateway to forward simulation data from one client to another.
Furthermore, it collects statistics, monitors the quality of service and handles encryption or tunneling through VPNs.

For optimal performance the server is implemented in low-level C and makes use of several Linux-specific realtime features.
The primary design goal was to make the behaviour of the system as deterministic as possible.

Therefore, it's advisable to run the server component on a [PREEMPT_RT](https://rt.wiki.kernel.org/index.php/CONFIG_PREEMPT_RT_Patch) patched version of Linux. In our environment, we use Fedora-based distribution which has been stripped to the bare minimum (no GUI, only a few background processes).

The server is a multi-threaded application.

### Clients

There are two types of clients:

1.  The server handles diffrent types of nodes.
    The most commonly used node-type is the 'socket' type which allows communication over network links (UDP, raw IP, raw Ethernet frames).
    But there are also other specialized node types to retreive or send data to equipemnt, which is directly connected to or running on the server itself.
    An example for such a node is the 'gtfpga' type which directly fetches and pushes data to a PCIe card.
    Or the 'file' type which logs or replays simulation data from the harddisk.

2. An other way to connect simulation equipment is by using a client-application which itself sends the data over the network to VILLASnode.
   In this scenario, VILLASnode uses the 'socket' node-type to communicate with the client-application.

Usually, new clients / equipemnt should be implemented as a new node-type as part of VILLASnode.
Using a dedicated client-application which communicates via the 'socket' type is deprecated because it leads to code duplication.

## Contact

[![EONERC ACS Logo](doc/pictures/eonerc_logo.png)](http://www.acs.eonerc.rwth-aachen.de)

- Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
- Marija Stevic <mstevic@eonerc.rwth-aachen.de>

Institute for Automation of Complex Power Systems (ACS) 
EON Energy Research Center (EONERC)
RWTH University Aachen, Germany
