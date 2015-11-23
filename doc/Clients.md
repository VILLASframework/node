# Clients

@subpage gtfpga
@subpage opal
@subpage file
@subpage socket
@subpage ngsi

Every server needs clients which act as sinks / sources for simulation data. In case of S2SS these clients are called _nodes_.

Take a look at the `clients/` directory for them:

 - RTDS via GTFPGA and UDP
 - RTDS via GTFPGA and PCIexpress
 - OPAL via Asynchronous Process and UDP
 - OPAL via Asynchronous Process and Shared memory
 - NGSI 9/10 (FIRWARE context broker)
 - Labview (*planned*)
 - Simulink (*planned*)
