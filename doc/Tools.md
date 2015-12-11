# Tools {#tools}

S2SS comes with a couple of tools to test and debug connectivity and configurations.
All S2SS tools are available as subcommands to the `s2ss` wrapper:
 
### `s2ss server`

Starts the simulator to simulator server. The server acts as a central gateway to forward simulation data.

    Usage: s2ss-server CONFIG
      CONFIG is a required path to a configuration file
    
    Supported node types:
     - file: support for file log / replay node type
     - socket: Network socket (libnl3)
     - gtfpga: GTFPGA PCIe card (libpci)
     - ngsi: OMA Next Generation Services Interface 10 (libcurl, libjansson, libuuid)
     - websocket: Send and receive samples of a WebSocket connection (libwebsockets)

### `s2ss pipe`

The `pipe` subcommand allows to read and write samples to `stdin` / `stdout`.

    Usage: s2ss-pipe CONFIG [-r] NODE
      CONFIG  path to a configuration file
      NODE    the name of the node to which samples are sent and received from
      -r      swap read / write endpoints)

### `s2ss signal`

The `signal` subcommand is a signal generator which writes samples to `stdout`.
This command can be combined with the `pipe` subcommand.

    Usage: s2ss-signal SIGNAL VALUES RATE [LIMIT]
      SIGNAL is on of: mixed random sine triangle square
      VALUES is the number of values a message contains
      RATE   how many messages per second
      LIMIT  only send LIMIT messages

### `s2ss test`

    Usage: s2ss-test CONFIG TEST NODE [ARGS]
      CONFIG  path to a configuration file
      TEST    the name of the test to execute: 'rtt'
      NODE    name of the node which shoud be used

## Examples

 1. Start server:

    $ s2ss server etc/example.conf

 2. Receive/dump data to file

    $ s2ss pipe etc/example.conf node_name > dump.csv

 3. Replay recorded data:

    $ s2ss pipe etc/example.conf -r node_name < dump.csv

 4. Send random generated values:

    $ s2ss signal random 4 100 | s2ss pipe etc/example.conf destination_node

 5. Test ping/pong latency:

    $ s2ss test latency etc/example.conf test_node