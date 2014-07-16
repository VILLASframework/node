S2SS is a client/server application based on UDP/IP to connect simulation equipment/software such as:

 - OPAL-RT,
 - RTDS,
 - Simulink,
 - LabView,
 - custom made equipment,
 - and FPGA models.

It's designed with focus on very low latency to achieve almost realtime communication.
S2SS is used in distributed- and co-simulation scenarios and developed for the field of power grid simulation at the EON Energy Research Centre in Aachen, Germany.

## Overview

The project consists of a server and several client applications.
Both server-to-server and direct client-to-client communication is possible.
All communication links use the same message protocol to exchange their measurement values.

### Server

For optimal performance the server is implemented in lowlevel C and makes use of several Linux-specific realtime features.

### Clients

## Contact

This project is developed at the [Institute for Automation of Complex Power Systems](www.acs.eonerc.rwth-aachen.de) (ACS) at the EON Energy Research Center (EONERC) at the [RWTH University](http://www.rwth-aachen.de) in Aachen.

 - Steffen Vogel <StVogel@eonerc.rwth-aachen.de>
 - Marija Stevic <MStevic@eonerc.rwth-aachen.de>


