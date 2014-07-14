# Network Emulation {#Netem}

S2SS supports the emulation of wide-area network characterisics.

For this an existing network emulator called 'netem' is used.
Netem is part of the Linux kernel and therefore shipped with most distributions by default.
Netem is implemented as a queuing discipline.
This discipline define the order and point in time when packets will be send over the network.

S2SS only takes care of setup and initalizing the netem queuing discipline inside the kernel.
For this the iproute2 software package (`ip` & `tc` commands) must be installed.
The configuration is done via the config file.
Look at `etc/example.conf` for a section called `netem` or `tc-netem(8)` for more details.

## Custom delay distributions

Netem supports loading custom delay distributions.

1. Load and compile the netem tools from:
   https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/netem
2. Create a custom distribution by following the steps described here:
   https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/README.distribution
3. Put the generated distrubtion with the suffix `.dist` in the `tc` lib directory:  `/usr/lib/tc/`.
4. Load the distribution specifying the basename in the server config.

## Links

 - http://www.linuxfoundation.org/collaborate/workgroups/networking/netem
 - https://git.kernel.org/cgit/linux/kernel/git/shemminger/iproute2.git/tree/README.iproute2+tc
