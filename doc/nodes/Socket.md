# Socket {#socket}

The socket node-type is the most comprehensive and complex one.
It allows to send and receive simulation data over the network.
Internally it uses the well known BSD socket API.

Please note that only datagram / packet, connection-less based network protocols are supported.
This means that there's currently no support for TCP!

The implementation supports multiple protocols / OSI layers:

 - Layer 1: Raw Ethernet Frames (no routing!)
 - Layer 2: Raw IP (internet / VPN routing possible)
 - Layer 3: UDP encapsulation

## Configuration

Every `socket` node supports the following special settings:

#### `local` *("ip:port" | "mac:protocol")*

#### `remote` *("ip:port" | "mac:protocol")*

#### `netem` *(dictionary)*

#### `layer` *("udp" | "ip" | "eth")*

### Example

	nodes = {
		udp_node = {					# The dictionary is indexed by the name of the node.
			type	= "socket",			# Type can be one of: socket, opal, file, gtfpga, ngsi
								# Start the server without arguments for a list of supported node types.
		
		### The following settings are specific to the socket node-type!! ###
	
			layer	= "udp"				# Layer can be one of:
								#   udp		Send / recv UDP packets
								#   ip		Send / recv IP packets
								#   eth		Send / recv raw Ethernet frames (IEEE802.3)
	
							
			local	= "127.0.0.1:12001",		# This node only received messages on this IP:Port pair
			remote	= "127.0.0.1:12000"		# This node sents outgoing messages to this IP:Port pair
		
			vectorize = 30				# Receive and sent 30 samples per message (multiplexing).
		}
	}

## Packet Format

The on-wire format of the network packets is not subject to a standardization process.
It's a very simple packet-based format which includes:

 - 32bit floating-point or integer values
 - 32bit timestamp (integral seconds)
 - 32bit timestamp (integral nanoseconds)
 - 16bit sequence number
 - 4bit version identified
 - and several flags...

## Message

A message contains a variable number of values.
Usually a a simulator sends one message per timestep.

Simulation data is encapsulated in UDP packages in sent over IP networks like the internet.
We designed a lightweight message format (or protocol) to facilitate a fast transmission.

@diafile msg_format.dia

For now, only the first message type (`data`) is used.
Therefore the complete protocol is **stateless**.
Later we might want to support more complex simulation scenarios which require some kind of controlling.

Except for the simulation data, all values are sent in **network byte order** (big endian)!
The endianess of the simulation data is indicated by a single bit in the message header.
This allows us to reduce the amount of conversions during one transfer.

@see msg for implementation details.

### Example

@todo add screenshot of wireshark dump

## Network Emulation {#netem}

S2SS supports the emulation of wide-area network characterisics.

This emulation can be configured on a per-node basis for **outgoing** / **egress** data only.
Incoming data is not processed by the network emulation!

This network emulation is handled by Linux' [netem queuing discipline](http://www.linuxfoundation.org/collaborate/workgroups/networking/netem) which is part of the traffic control subsystem.
Take a look at the following manual page for supported metrics: [tc-netem(8)](http://man7.org/linux/man-pages/man8/tc-netem.8.html).

S2SS only takes care of setup and initalizing the netem queuing discipline inside the kernel.
For this the iproute2 software package (`ip` & `tc` commands) must be installed.
The configuration is done via the config file.
Look at `etc/example.conf` for a section called `netem` or `tc-netem(8)` for more details.

### Custom delay distribution

Netem supports loading custom delay distributions.

1. Load and compile the netem tools from:
   https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/netem
2. Create a custom distribution by following the steps described here:
   https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/README.distribution
3. Put the generated distrubtion with the suffix `.dist` in the `tc` lib directory:  `/usr/lib/tc/`.
4. Load the distribution specifying the basename in the server config.

### Further information

 - https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/README.iproute2+tc
 - https://github.com/stv0g/netem

