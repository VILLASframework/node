# Tools {#tools}

S2SS comes with a couple of tools to test and debug:

 1. `send` can be used to send samples from STDIN to a node
 2. `receive` can be used to read samples from a node and write it to STDOUT
 3. `random` writes random sample data to STDOUT

## Examples

 1. Start server:

    $ ./server etc/example.conf

 2. Receive/dump data to file

    $ ./receive etc/example.conf node_name > dump.csv

 3. Replay recorded data:

    $ ./send etc/example.conf -r node_name < dump.csv

 4. Send random generated values:

    $ ./random 4 100 | sudo ./send etc/example.conf destination_node

 5. Test ping/pong latency:

    $ ./test latency etc/example.conf test_node