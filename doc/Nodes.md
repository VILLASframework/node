# Node-types {#nodes}

Every server needs clients which act as sinks / sources for simulation data. In case of S2SS these clients are called _nodes_.
Every node is an instance of a node-type. S2SS currently supports the following node-types:

#### @subpage gtfpga
 - RTDS via GTFPGA and PCIexpress (Linux vfio, uio)
 
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