# Socket

The socket node type is the most comprehensive one. It allows to send / receive simulation data over the network.
Internally it uses the well known BSD sockets.

The implementation allows to chose the OSI layer which should:

 - Layer 1: Raw Ethernet Frames (no routing!)
 - Layer 2: Raw IP (internet / VPN routing possible)
 - Layer 3: UDP encapsulation
 
 