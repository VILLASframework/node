---
title: 'VILLASnode: An Open-Source Real-time Multi-protocol Gateway'
tags:

- C/C++
- Real-time
- Distributed experiments
- Electrical grid simulation

authors:

- name: Steffen Vogel
  orcid: 0000-0003-3384-6750
  affiliation: "1, 3"
- name: Niklas Eiling
  orcid: 0000-0002-7011-9846
  affiliation: 1
- name: Manuel Pitz
  orcid: 0000-0002-6252-2029
  affiliation: 1
- name: Alexandra Bach
  orcid: 0009-0005-7385-4642
  affiliation: 1
- name: Marija Stevic
  affiliation: "1, 3"
- name: Prof. Antonello Monti
  orcid: 0000-0003-1914-9801
  affiliation: "1, 2"

affiliations:

- name: Institute for Automation of Complex Power Systems, RWTH Aachen University, Germany
  index: 1
- name: Fraunhofer Institute for Applied Information Technology, Aachen, Germany
  index: 2
- name: OPAL-RT Germany GmbH
  index: 3

date: 20 June 2025
bibliography: paper.bib
---

# Summary

VILLASnode is a multi-protocol gateway, designed to facilitate real-time data exchange between various components of geographically distributed real-time experiments. Components can be test beds, digital real-time simulators, software tools, and physical devices. It is designed for the co-simulation of power system and related energy applications.
VILLASnode was originally designed for co-simulation of electrical networks, but was developed to bigger variety of use cases and domains.
Whereas distributed computing, systems or algortihms aim to solve a common task, geographically distributed experiments link infrastructures with different components to make these components accessible to other infrastructures. Thus, data exchanges are possible which would not be possible in one single infrastructure.
VILLASnode serves as the gateway that connects components across different infrastructures by providing a set of protocols and customized third-party implementations, e.g., different simulators. It enables seamless collaboration in research and testing environments while safeguarding the intellectual property of the infrastructures. The components at every infrastructure appear as a black box. The infrastructure does not need to share models or confidential information.

VILLASnode is a Linux command line tool and can run as installation from source or as container. It is written in C/C++ and designed in a modular and extensible way.
All components which are interfaced by the VILLASnode gateway are represented by nodes ($n$). These nodes act as sinks or sources for data specific to the component. Every node is an instance of a node-type. In a single VILLASnode instance, multiple instances of the same node-type can co-exist at the same time.
The basic data package, common for all node-types, includes timestamped data, constituting a sample. Up to 64 values can form a sample.
Samples may need modification or filtering. VILLASnode supports hooks ($h$) for this purpose. Hooks are simple callback functions, which are called whenever a message is processed.
Paths ($p$) take care of the processing and define the connections and dataflows between nodes.
Node-types, hooks, and paths need to be initalized in a JSON configuration file which is passed when starting VILLASnode.
Figure 1 shows an example of an experiment where five different node-types are used, connected by three paths, using three hooks.
It includes queues ($q$) and registers ($r$). Queues temporarily store data before data is forwarded to registers. Registers provide the possibility to (de-)mulitplex data and to create new samples.

![Example of modular experimental design with nodes, paths, and hooks [@villasnode_docs].](figures/VILLASnode_paths.svg)

External software tools and programming languages can be interfaced with VILLASnode via several methods: Locally, they can be spawned as sub-processes and exchange data via shared-memory or standard I/O streams. Remotely, they can be interfaced via any of the supported node-types and protocols, such as MQTT or e.g. plain TCP sockets.
Additionally, VILLASnode can be configured and controlled remotely by an HTTP REST-style Application Programming Interface (API). An OpenAPI specification of the API is provided in order to generate API bindings for a variety of different programming languages.
Since VILLASnode is written in C/C++ it supports real-time capabilites and can supports real-time tuning for Linux systems.
To provide all necassary information, VILLASnode has a detailed documentation [@villasnode_docs]. It includes installation recommendations and best practices for development as well as example configurations and beginners guides, so-called labs.

Other open-source co-simulation tools are available like Mosaik [@rohjans_mosaik_2013] or Helics [@hardy_helics_2024]. Mosaik uses a SimAPI to communicate with other simulators based on fixed timesteps. Integrated simulators need to support the SimAPI which requires extra implementations. Simulators cannot reuse existing supported protocols which VILLASnode makes use of and takes core of protocol conversion.
Helics is designed for large-scale testing and supports bindings for different languages. It uses a broker which manages the communication between the simulators, whereas VILLASnode has a peer-to-peer architecture.

# Statement of need

VILLASnode supports the coupling of real-time simulators of different hardware and software vendors, specifications and implementations [@monti_global_2018]. They play a crucial role in both academic research and industrial applications, especially within simulation of electrical power networks. They are primarily utilized for hardware-in-the-loop (HiL) simulations. As an example, in the scope of the ENSURE project, the German electrical grid is emulated with the ability to couple dynamically simulators and physical components which can be interfaced safely via analog input and output signals [@Mehlmann2023].
This approach not only reduces development costs but also allows for the validation of components under scenarios that are difficult or unsafe to replicate real-world field scenarios. Moreover, VILLASnode supports the linking of simulators and real-time simulators of different vendors so that models do not need to be shared. It allows for conversation of intellectual property during collaboration. Local simulator and simulation infrastructure can be combined in a powerful arrangement unavailable at any individual infrastructure. This approach has the additional advantage of allowing to co-simulate with heterogeneus methods, algorithms and communication protocols. The communication with the components, sensors or actuators can also be implemented using VILLASnode.

This can be extended to geographically distributed experiments. In this case, VILLASnode takes care of the communication between the different participants to the experiment which spans multiple infrastructures. Not only simulators can be included, but also physical devices can be integrated [@Bach2025]. Although communication latencies constrain the dynamics of the experiment for real-time applications, VILLASnode offers significant advantages such as leveraging existing infrastructure across sites and facilitating collaboration among interdisciplinary teams. Manufacturers, customers, and certifiers benefit from remote access and distributed testing while maintaining data confidentiality and intellectual property. Current work includes automation of integration of VILLASnode in practical field devices [@Pitz2024].

# Features

VILLASnode is a part of the VILLASframework, which offers several key features.
Firstly, it includes VILLASweb, a tool designed for the visualization and monitoring of experiments.
Secondly, there is VILLASfpga, which provides hard real-time capabilities for VILLASnode.
Lastly, VILLAScontroller is currently under development and aims to automate simulation equipment.

The gateway supports a wide range of established data exchange protocols. Support for various message brokers like MQTT, AMQP, nanomsg, ZeroMQ, Redis or Kafka provide connectivity to existing software platforms. Support for process bus protocols such as CAN, EtherCAT, Modbus, IEC 61850-8-1 (GOOSE), IEC 61850-9-2 (Sampled Values) enable connectivity to physical devices. A range of web-based protocols like WebRTC, WebSockets, NGSI or a HTTP REST API facilitate integration into web-based interfaces. Lastly, a set of generic protocols add support for plain UDP/TCP/Ethernet sockets, Infiniband, file-based I/O or communication with other processes via standard I/O or shared memory regions.

# Acknowledgements

We like to thank several colleagues and students who supported us. Especially, we thank Leonardo Carreras, Laura Fuentes Grau, Andres Acosta, and Felix Wege.

- [RESERVE](http://re-serve.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 727481.
- [VILLAS](https://villas.fein-aachen.org/website/): Funding provided by [JARA-ENERGY](http://www.jara.org/en/research/energy). Jülich-Aachen Research Alliance (JARA) is an initiative of RWTH Aachen University and Forschungszentrum Jülich.
- [Urban Energy Lab 4.0](https://www.uel4-0.de/Home/): Funding is provided by the [European Regional Development Fund (EFRE)](https://ec.europa.eu/regional_policy/en/funding/erdf/).
- [ERIGrid2.0](https://erigrid2.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 870620.
- [ENSURE](https://www.kopernikus-projekte.de/projekte/ensure): Federal Ministry of Education and Research supports the project under identification 03SFK1C0-3.
- [NFDI4Energy](https://nfdi4energy.uol.de/): Funded by the Deutsche Forschungsgemeinschaft (DFG, German Research Foundation) – 501865131.
- [HYPERRIDE](https://hyperride.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 957788.
- [AI-EFFECT](https://ai-effect.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 101172952.
- [EnerTEF](https://enertef.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 101172887.
- [Target-X](https://target-x.eu/): European Unions Horizon 2020 research and innovation programme under grant agreement No. 101096614.

# References
