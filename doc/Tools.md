# Tools {#tools}

VILLASnode comes with a couple of tools to test and debug connectivity and configurations.
All VILLASnode tools are available as subcommands to the `villas` wrapper:
 
### `villas node`

Starts the simulator to simulator server. The server acts as a central gateway to forward simulation data.

    Usage: villas-node CONFIG
      CONFIG is a required path to a configuration file
    
    Supported node types:
     - file: support for file log / replay node type
     - socket: Network socket (libnl3)
     - gtfpga: GTFPGA PCIe card (libpci)
     - ngsi: OMA Next Generation Services Interface 10 (libcurl, libjansson, libuuid)
     - websocket: Send and receive samples of a WebSocket connection (libwebsockets)

### `villas pipe`

The `pipe` subcommand allows to read and write samples to `stdin` / `stdout`.

    Usage: villas-pipe CONFIG [-r] NODE
      CONFIG  path to a configuration file
      NODE    the name of the node to which samples are sent and received from
      -r      swap read / write endpoints)

### `villas signal`

The `signal` subcommand is a signal generator which writes samples to `stdout`.
This command can be combined with the `pipe` subcommand.

    Usage: villas-signal SIGNAL [OPTIONS]
      SIGNAL   is on of: 'mixed', 'random', 'sine', 'triangle', 'square', 'ramp'
      -v NUM   specifies how many values a message should contain
      -r HZ    how many messages per second
      -f HZ    the frequency of the signal
      -a FLT   the amplitude
      -d FLT   the standard deviation for 'random' signals
      -l NUM   only send LIMIT messages and stop

### `villas test`

    Usage: villas-test CONFIG TEST NODE [ARGS]
      CONFIG  path to a configuration file
      TEST    the name of the test to execute: 'rtt'
      NODE    name of the node which shoud be used
 
### `villas fpga`

    Usage: ./fpga CONFIGFILE CMD [OPTIONS]
       Commands:
          tests      Test functionality of VILLASfpga card
          benchmarks Do benchmarks
    
       Options:
          -d    Set log level

## Examples

 1. Start server:

    $ villas node etc/example.conf

 2. Receive/dump data to file

    $ villas pipe etc/example.conf node_name > dump.csv

 3. Replay recorded data:

    $ villas pipe etc/example.conf -r node_name < dump.csv

 4. Send random generated values:

    $ villas signal random 4 100 | villas pipe etc/example.conf destination_node

 5. Test ping/pong latency:

    $ villas test latency etc/example.conf test_node