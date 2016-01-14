# S2SS clients

This directory contains code and projects to connect various simulators and tools to the S2SS server.
Usually, this are projects which are not based on the S2SS source code but which are using the same protocol as defined by the `socket` node-type.
For a protocol specification, please see `include/msg_format.h`.

- opal/udp
   Contains the implementation of an asynchronous process block for RT-LAB.
   This block allows exchanging sample values with S2SS over UDP using the `socket` node-type.
   Author: Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

- labview
   This example model is using LabView standard UDP blocks to exchange sample values with S2SS over UDP using the `socket` node-type.
   Author: Eyke Liegmann <ELiegmann@eonerc.rwth-aachen.de>
