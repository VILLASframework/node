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

A path is a **unidirectional** connection between incoming and outgoing nodes.

A path simply forwards messages from the incoming node to the outgoing node.
By default the server does not alter message contents.
It only checks if the message headers are valid (sequence number, cryptographic signature..)
However every path supports optional callback function which allow user-defined operations on the message contents.

For bidirectional communication a corresponding path in the reverse direction must be added.

The server is designed to allow multiple outgoing and incoming nodes per path as an extension.
@todo This has **not** been implemented at the moment but will come in future versions.

@diafile path_simple.dia

@see path for implementation details.

## Message

A message contains a variable number of values.
Usually a a simulator sends one message per timestep.

Simulation data is encapsulated in UDP packages in sent over IP networks like the internet.
We designed a lightweight message format (or protocol) to facilitate a fast transmission.

@diafile msg_format.dia

### Protocol details

There are several types of messages:

  * **Data**: contains simulation data.
  * **Control**: start/stops a new simulation case.
  * **Config**: used to pass settings between nodes.
  * **Sync**: used to syncrhonize the clocks between nodes.

For now, only the first type (data) is implemented.
Therefore the complete protocol is **stateless**.
Later we might want to support more complex simulation scenarios which require some kind of controlling.

Except for the simulation data, all values are sent in **network byte order** (big endian)!
The endianess of the simulation data is indicated by a single bit in the message header.
This allows us to reduce the amount of conversions during one transfer.

@see msg for implementation details.

## Interface

@todo Add documentation

@see interface for implementation details.
