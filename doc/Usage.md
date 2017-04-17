# Usage {#usage}

# `villas signal` 

# `villas pipe`
 
# `villas hook`

## `villas node`

The core of VILLASnode is the `villas-node` daemon.
The folling usage information is provided when called like `villas-node --help`;

```
Usage: villas-node [CONFIG]
  CONFIG is the path to an optional configuration file
         if omitted, VILLASnode will start without a configuration
         and wait for provisioning over the web interface.

Supported node types:
 - file        : support for file log / replay node type
 - cbuilder    : RTDS CBuilder model
 - socket      : BSD network sockets
 - fpga        : VILLASfpga PCIe card (libxil)
 - ngsi        : OMA Next Generation Services Interface 10 (libcurl, libjansson)
 - websocket   : Send and receive samples of a WebSocket connection (libwebsockets)

Supported hooks:
 - restart     : Call restart hooks for current path
 - print       : Print the message to stdout
 - decimate    : Downsamping by integer factor
 - fix_ts      : Update timestamps of sample if not set
 - skip_first  : Skip the first samples
 - drop        : Drop messages with reordered sequence numbers
 - convert     : Convert message from / to floating-point / integer
 - shift       : Shift the origin timestamp of samples
 - ts          : Update timestamp of message with current time
 - stats       : Collect statistics for the current path
 - stats_send  : Send path statistics to another node

Supported API commands:
 - nodes       : retrieve list of all known nodes
 - config      : retrieve current VILLASnode configuration
 - reload      : restart VILLASnode with new configuration

VILLASnode v0.7-0.2-646-g59756e7-dirty-debug (built on Mar 12 2017 21:37:40)
 copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 Steffen Vogel <StVogel@eonerc.rwth-aachen.de>
```

The server requires root privileges for:

 - Enable the realtime FIFO scheduler
 - Increase the task priority
 - Configure the network emulator (netem)
 - Change the SMP affinity of threads and network interrupts