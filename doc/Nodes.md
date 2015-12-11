# Nodes {#nodes}

Every server needs clients which act as sinks / sources for simulation data. In case of S2SS these clients are called _nodes_.
Every node is an instance of a node type. S2SS currently supports the following node types:

@subpage gtfpga
@subpage opal
@subpage file
@subpage socket
@subpage websocket
@subpage ngsi

Use cases:

 - RTDS via GTFPGA and UDP (`socket`)
 - RTDS via GTFPGA and PCIexpress (`gtfpga`)
 - OPAL via Asynchronous Process and UDP (`socket`)
 - OPAL via Asynchronous Process and Shared memory (`opal`)
 - NGSI 9/10 (FIRWARE context broker, `ngsi`)
 - WebSockets for live monitoring (`websocket`)
 - Labview (*planned*)
 - Simulink (*planned*)
