# Concept

The server is designed around the concept of _nodes_ and _paths_.
It's the task of the server to forward real-time simulation data between multiple parties.
In doing so, the server has to perform simple checks and collects statistics.
From the viewpoint of the communication parters the server is nearly transparent.
Hence, it's cruical to keep the added overhead as low as possible (in terms of latency).

## Nodes

All communication partners are represented by nodes.

Possible types of nodes are:
  * Simulators (OPAL, RTDS)
  * Servers (S2SS instances)
  * Workstations (Simulink, Labview, ...)
  * Data Loggers
  * etc..

@see node for implementation details.

## Paths

A path is a **uni-directional** connection between incoming and outgoing nodes.

It forwards messages from a single incoming node to multiple outgoing nodes.
Therefore it represents a 1-to-n relation between nodes.

For bidirectional communication a corresponding path in the reverse direction must be added.
 
By default, message contents are not altered.
The server only performs checks for valid message headers (sequence number, cryptographic signature..).
However every path supports optional hook/callback functions which allow user-defined operations on the message contents.

@diafile path_simple.dia

@see path for implementation details.

## Interface

Interfaces are used to represent physical network ports on the server.
They are only used by the [Socket](socket) type of nodes.

@todo Add documentation

@see interface for implementation details.
