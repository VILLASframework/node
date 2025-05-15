<!--
Author: Alexandra Bach <alexandra.bach@eonerc.rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
-->

---
title: 'VILLASnode: Open-source real-time coupling of distributed infrastructures'
tags:

- C/C++
- Real-time
- Distributed experiments
- Electrical grid simulation

authors:

- name: Alexandra Bach
  orcid: 0009-0005-7385-4642
  affiliation: 1
- name: Leonardo Carreras
  orcid: 0000-0002-9033-1051
  affiliation: 1
- name: Laura Fuentes Grau
  orcid: 0009-0004-8997-7009
- name: Steffen Vogel
  orcid: 0000-0003-3384-6750
  affiliation: "1, 3" # (Multiple affiliations must be quoted)
- name: Manuel Pitz
  orcid: 0000-0002-6252-2029
  affiliation: 1
- name: Niklas Eiling
  orcid: 0000-0002-7011-9846
  affiliation: 1
- name: Felix Wege
  orcid: 0000-0001-6602-9875
  affiliation: 1
- name: Andres Acosta
  orcid: 0000-0003-3066-8354
  affiliation: 1
- name: Iris Köster
  affiliation: 1
- name: Prof. Antonello Monti
  orcid: 0000-0003-1914-9801
  affiliation: "1,2"

affiliations:

- name: Institute for Automation of Complex Power Systems, RWTH Aachen University, Germany
  index: 1
- name: Fraunhofer Institute for Applied Information Technology, Aachen, Germany
  index: 2
- name: OPAL-RT Germany GmbH
  index: 3

date: 15 May 2025
bibliography: paper.bib

---

# Summary

VILLASnode is a software tool, designed to facilitate real-time data exchange between various components in geographically distributed real-time experiments. Components can be test beds, real-time simulators, software tools, and physical devices.
Whereas distributed computing, systems or algortihms aim to solve a common task, geographically distributed experiments link infrastructures with different components to make these components accessible to other infrastructures. Thus, data exchanges are possible which would not be possible in one single infrastructure.
VILLASnode serves as the gateway that connects components across different infrastructures by providing a set of protocols and customized third-party implementations, e.g., different simulators. It enables seamless collaboration in research and testing environments while safeguarding the intellectual property of the infrastructures. The components at every infrastructure appear as a black box. The infrastructure does not need to share models or confidential information.

VILLASnode is a Linux command line tool and can run as installation from source or as container. It is written in C/C++ and designed in a modular way.
All components which are interfaced by the VILLASnode gateway are represented by nodes. These nodes act as sinks or sources for data specific to the component. Every node is an instance of a node-type. In a single VILLASnode instance, multiple instances of the same node-type can co-exist at the same time.
The basic data package, common for all node-types, includes timestamped data, constituting a sample. Up to 64 values can form a sample.
Samples may need modification or filtering. VILLASnode supports hooks for this purpose. Hooks are simple callback functions, which are called whenever a message is processed.
Paths take care of the processing and define the connections and dataflows between nodes.
Node-types, hooks, and paths need to be initalized in a configuration file which is passed when starting VILLASnode.
VILLASnode can be controlled remotely by an Application Programming Interface (API). It is used by the Python Wrapper allowing to control, configure and execute most of the functions of VILLASnode. This is useful if Python software tools are to be integrated in a distributed experiment.
In general, the interaction with VILLASnode is available for other tools and coding languages by using clients that implement basic functionalities, custom configurations and data conversions.
Since VILLASnode is written in C/C++ it supports real-time capabilites and can supports real-time tuning for Linux systems.

# Statement of need

VILLASnode supports the coupling of real-time simulators of different hardware and software vendors, specifications and implementations [@monti_global_2018]. They play a crucial role in both academic research and industrial applications, especially within simulation of electrical power networks. They are primarily utilized for hardware-in-the-loop (HiL) simulations. For example, an electrical grid is emulated so that the physical component can interface safely via analog input and output signals [@Mehlmann2023].
This approach not only reduces development costs but also allows for the validation of components under scenarios that are difficult or unsafe to replicate real-world field scenarios. Moreover, VILLASnode supports the linking of simulators and real-time simulators of different vendors so that models do not need to be shared. It allows for conversation of intellectual property during collaboration. Local simulator and simulation infrastructure can be combined in a powerful arrangement unavailable at any individual infrastructure. This approach has the additional advantage of allowing to co-simulate with heterogeneus methods, algorithms and communication protocols. The communication with the components, sensors or actuators can also be implemented using VILLASnode.

This can be extended to geographically distributed experiments. In this case, VILLASnode takes care of the communication between the different participants to the experiment which spans multiple infrastructures. Not only simulators can be included, but also physical devices can be integrated [@Bach2025]. Although communication latencies constrain the dynamics of the experiment for real-time applications, VILLASnode offers significant advantages such as leveraging existing infrastructure across sites and facilitating collaboration among interdisciplinary teams. Manufacturers, customers, and certifiers benefit from remote access and distributed testing while maintaining data confidentiality and intellectual property. Current work includes automation of integration of VILLASnode in practical field devices [@Pitz2024].

# Features

VILLASnode is a part of the VILLASframework, which offers several key features.
Firstly, it includes VILLASweb, a tool designed for the visualization and monitoring of experiments.
Secondly, there is VILLASfpga, which provides hard real-time capabilities for VILLASnode.
Lastly, VILLAScontroller is currently under development and aims to automate simulation equipment.

# Acknowledgements

- [RESERVE](http://re-serve.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 727481.
- [VILLAS](https://villas.fein-aachen.org/website/): Funding provided by [JARA-ENERGY](http://www.jara.org/en/research/energy). Jülich-Aachen Research Alliance (JARA) is an initiative of RWTH Aachen University and Forschungszentrum Jülich.
- [Urban Energy Lab 4.0](https://www.uel4-0.de/Home/): Funding is provided by the [European Regional Development Fund (EFRE)](https://ec.europa.eu/regional_policy/en/funding/erdf/).
- [ERIGrid2.0](https://erigrid2.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 870620.
- [ENSURE](https://www.kopernikus-projekte.de/projekte/ensure): Federal Ministry of Education and Research supports the project under identification 03SFK1C0-3.
- [NFDI4Energy](https://nfdi4energy.uol.de/): Funded by the Deutsche Forschungsgemeinschaft (DFG, German Research Foundation) – 501865131.
- [HYPERRIDE](https://hyperride.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 957788.
- [Erigrid2.0](https://erigrid2.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 870620.
- [AI-EFFECT](https://ai-effect.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 101172952.
- [EnerTEF](https://enertef.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 101172887.

# References
