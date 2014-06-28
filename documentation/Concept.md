# Concept

The server is designed around the concept of _nodes_ and _paths_.

A path is a **unidirectional** connection between two nodes.

Messages from the incoming node get forwarded to the outgoing node.

For bidirectional communication a corresponding path in the reverse direction must be added.

The server is designed to allow multiple outgoing nodes as an extension.
@todo This has **not** been implemented at the moment!

\diafile path_simple.dia

## Path

@todo Add documentation

@see path

## Node

@todo Add documentation

@see node

## Message

@todo Add documentation

A message contains a variable number of values.
Usually a a simulator sends one message per timestep.

The format of a message is defined by the following structure:

~~~{.c}
struct msg
{
	/** Sender device ID */
	uint16_t device;
	/** Message ID */
	uint32_t sequence;
	/** Message length (data only) */
	uint16_t length;
	/** Message data */
	double data[MAX_VALUES];
} __attribute__((packed));
~~~

@see msg

## Interface

@todo Add documentation

@see interface
