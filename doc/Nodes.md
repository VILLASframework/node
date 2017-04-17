# Node-types {#nodes}

Every server needs clients which act as sinks / sources for simulation data. In case of VILLASnode these clients are called _nodes_.
Every node is an instance of a node-type. VILLASnode currently supports the following node-types:

#### @subpage villasfpga
 - VILLASfpga sub-project connect RTDS via GTFPGA and PCIexpress (Linux vfio, uio)
 
#### @subpage opal
 - OPAL via Asynchronous Process (libOpalAsyncApi)

#### @subpage file
 - Log & replay of sample values
 - Static load profiles

#### @subpage socket
 - RTDS via GTFPGA and UDP
 - OPAL via Asynchronous Process and UDP

#### @subpage websocket
 - WebSockets for live monitoring and user interaction

#### @subpage ngsi
 - NGSI 9/10 a.k.a. FIRWARE context broker

#### @subpage labview
 - NI LabView RT-targets
 
#### @subpage cbuilder
- RTDS CBuilder Control System components

#### @subpage shmem
- POSIX shared memory interface
