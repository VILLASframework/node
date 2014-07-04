# Concept

The server is designed around the concept of _nodes_ and _paths_.

A path is a **unidirectional** connection between two nodes.

Messages from the incoming node get forwarded to the outgoing node.

For bidirectional communication a corresponding path in the reverse direction must be added.

The server is designed to allow multiple outgoing nodes as an extension.
@todo This has **not** been implemented at the moment!

@diafile path_simple.dia

## Path

@todo Add documentation

@see path For a detailed descriptions of the fields.

## Node

@todo Add documentation

@see node For a detailed descriptions of the fields.

## Message

@todo Add documentation

@diafile msg_format.dia

A message contains a variable number of values.
Usually a a simulator sends one message per timestep.

@see msg For a detailed descriptions of the fields.

## Interface

@todo Add documentation

@see interface For a detailed descriptions of the fields.
