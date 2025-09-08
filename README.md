# <img src="doc/pictures/villas_node.png" width=40 /> VILLASnode

[![build status](https://git.rwth-aachen.de/acs/public/villas/node/badges/master/pipeline.svg)](https://git.rwth-aachen.de/acs/public/villas/node/-/pipelines/)
[![status](https://joss.theoj.org/papers/37c2509d36586f4cec2885d5c2088e8f/status.svg)](https://joss.theoj.org/papers/37c2509d36586f4cec2885d5c2088e8f)

This is VILLASnode, a gateway for processing and forwarding simulation data between real-time simulators.
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

## Related Projects

- [MIOB](https://github.com/RWTH-ACS/miob)
- [DINO](https://github.com/RWTH-ACS/dino)

## Contributing

All contributions are welcome!
If you want to contribute to VILLASnode, please visit the [contribution guidelines](https://villas.fein-aachen.org/docs/node/development/contributing/) in our documentation.

## License

VILLASnode is licensed on a per-file basis under multiple open-source licenses.
Each source file contains an SPDX license identifier and copyright attribution in accordance with the [REUSE Software guidelines](https://reuse.software/).
For attribution, please refer to the SPDX headers in each file.
The [Apache 2.0 license](./LICENSE) covers most of the codebase and is preferred for new contributions.
License compliance is automatically checked in CI, including validation of licenses in the `LICENSES/` directory.

We kindly ask all academic publications employing components of VILLASframework to cite the following paper:

- S. Vogel, N. Eiling, M. Pitz, A. Bach, M. Stevic, and P. A. Monti, “VILLASnode: An Open-Source Real-time Multi-protocol Gateway,” Journal of Open Source Software, vol. 10, no. 112, p. 8401, Aug. 2025, doi: [10.21105/joss.08401](https://doi.org/10.21105/joss.08401).

More publications using, or about VILLASnode [can be found in the documentation](https://villas.fein-aachen.org/docs/publications/).

For other licensing options please consult [Prof. Antonello Monti](mailto:amonti@eonerc.rwth-aachen.de).

- SPDX-FileCopyrightText: 2014-2025 The VILLASframework Authors
- SPDX-License-Identifier: Apache-2.0

## Contact

[![EONERC ACS Logo](doc/pictures/eonerc_logo.png)](http://www.acs.eonerc.rwth-aachen.de)

- Steffen Vogel <post@steffenvogel.de>
- Niklas Eiling <niklas@eil.ing>
- Felix Wege <fwege@eonerc.rwth-aachen.de>
- Alexandra Bach <alexandra.bach@eonerc.rwth-aachen.de>

[Institute for Automation of Complex Power Systems (ACS)](http://www.acs.eonerc.rwth-aachen.de)
[EON Energy Research Center (EONERC)](http://www.eonerc.rwth-aachen.de)
[RWTH University Aachen, Germany](http://www.rwth-aachen.de)
