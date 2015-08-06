# Socket

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

`local`

`remote`

`netem`

`layer`

### Example

@todo Add excerpt from example.conf

## Packet Format

@todo add DIA figure here

### Example

@todo add screenshot of wireshark dump

## Network Emulation
