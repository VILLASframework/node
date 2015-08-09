# Configuration

The S2SS configuration is completly contained in a single file.
Take a look at the example configuration: `server/etc/example.conf`.

The configuration file consists of three sections:

## Global

The global section consists of some global configuration parameters:

#### `debug`

`debug` expects an integer number (0-10) which determines the verbosity of debug messages during the execution of the server.
Use this with care! Producing a lot of IO might decrease the performance of the server.
Omitting this setting or setting it to zero will disable debug messages completely.

#### `stats`

`stats` specifies the rate in which statistics about the actives paths will be printed to the screen.
Setting this value to 5, will print 5 lines per second.
A line of includes information such as:
  - Source and Destination of path
  - Messages received
  - Messages sent
  - Messaged dropped

#### `affinity`

The `affinity` setting allows to restrict the exeuction of the daemon to certain CPU cores.
This technique, also called 'pinning', improves the determinism of the server by isolating the daemon processes on exclusive cores.

#### `priority`

The `priority` setting allows to adjust the scheduling priority of the deamon processes.
By default, the daemon uses a real-time optimized FIFO scheduling algorithm.

## Nodes

The node section is a **directory**	 of nodes (clients) which are connected to the S2SS instance.
The directory is indexed by the name of the node:

    nodes = {
        "sintef_node" = {
	    type = "socket"
	    ....
	}
    }

There are multiple diffrent type of nodes. But all types have the following settings in common:

#### `type`

`type` sets the type of the node. This should be one of:
  - `socket` which refers to a [Socket](socket) node.
  - `gtfpga` which refers to a [GTFPGA](gtfpga) node.
  - `opal` which refers to a [OPAL Asynchronous Process](opal) node.
  - `file` which refers to a [File](file) node.

The remaining settings per node a depending on `type`.
Take a look a the specific pages for details.

## Paths

The path section consists of a **list** of paths.
Every path is allowed to have the following settings:

The `in` and `out` settings expect the name of the source and destination node.
The `out` setting itself is allowed to be list of nodes.
This enables 1-to-n distribution of simulation data.

The optional `enabled` setting can be used to temporarily disable a path.
If omitted, the path is enabled by default.

By default, the path is unidirectional. Meaning, that it only forwards samples from the source to the destination.
Sometimes a bidirectional path is needed.
This can be accomplished by setting `reverse` to `true`.
