# <img src="doc/pictures/villas_node.png" width=40 /> VILLASnode

[![build status](https://git.rwth-aachen.de/acs/public/villas/node/badges/master/pipeline.svg)](https://git.rwth-aachen.de/acs/public/villas/node/-/pipelines/)

This is VILLASnode, a gateway for processing and forwardning simulation data between real-time simulators.
VILLASnode is a client/server application to connect simulation equipment and software such as:

 - OPAL-RT RT-LAB,
 - RTDS GTFPGA cards,
 - RTDS GTWIF cards,
 - Simulink,
 - LabView,
 - and FPGA models

by using protocols such as:

 - IEEE 802.2 Ethernet / IP / UDP,
 - ZeroMQ & nanomsg,
 - MQTT & AMQP
 - WebSockets
 - Shared Memory
 - Files
 - IEC 61850 Sampled Values / GOOSE
 - Analog/Digital IO via Comedi drivers
 - Infiniband (ibverbs)

It's designed with a focus on very low latency to achieve real-time exchange of simulation data.
VILLASnode is used in distributed- and co-simulation scenarios and developed for the field of power grid simulation at the EON Energy Research Center in Aachen, Germany.

## Documentation

User documentation is available here: <https://villas.fein-aachen.org/docs/>

## License

This project is released under the terms of the [Apache 2.0 license](LICENSE).

We kindly ask all academic publications employing components of VILLASframework to cite one of the following papers:

- A. Monti et al., "[A Global Real-Time Superlab: Enabling High Penetration of Power Electronics in the Electric Grid](https://ieeexplore.ieee.org/document/8458285/)," in IEEE Power Electronics Magazine, vol. 5, no. 3, pp. 35-44, Sept. 2018.
- S. Vogel, M. Mirz, L. Razik and A. Monti, "[An open solution for next-generation real-time power system simulation](http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=8245739&isnumber=8244404)," 2017 IEEE Conference on Energy Internet and Energy System Integration (EI2), Beijing, 2017, pp. 1-6.

For other licensing options please consult [Prof. Antonello Monti](mailto:amonti@eonerc.rwth-aachen.de).

- SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
- SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
- SPDX-License-Identifier: Apache-2.0

## Contact

[![EONERC ACS Logo](doc/pictures/eonerc_logo.png)](http://www.acs.eonerc.rwth-aachen.de)

- Steffen Vogel <post@steffenvogel.de>
- Marija Stevic <mstevic@eonerc.rwth-aachen.de>

[Institute for Automation of Complex Power Systems (ACS)](http://www.acs.eonerc.rwth-aachen.de)  
[EON Energy Research Center (EONERC)](http://www.eonerc.rwth-aachen.de)  
[RWTH University Aachen, Germany](http://www.rwth-aachen.de)  
