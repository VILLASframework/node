# Usage Instructions {#usage}

The S2SS server (`server`) expects the path to a configuration file as a single argument.

	Usage: ./server CONFIG
	  CONFIG is a required path to a configuration file

	Simulator2Simulator Server 0.1-d7de19c (Jun  4 2014 02:50:13)
	 Copyright 2014, Institute for Automation of Complex Power Systems, EONERC
	   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

The server requires root privileges for:

 - Enable the realtime fifo scheduler
 - Increase the task priority
 - Configure the network emulator (netem)
 - Change the SMP affinity of threads and network interrupts

## Step-by-step

1. Start putty.exe (SSH client for Windows)

  - Should be already installed on most systems
  - Or load .exe from this website (no installation required)
	    http://the.earth.li/~sgtatham/putty/latest/x86/putty.exe

2. Connect to S2SS server

| Setting  | Value          |
| :------- | :------------- |
| IP       | 130.134.169.31 |
| Port     | 22             |
| Protocol | SSH            |
| User     | acs-admin      |

3. Go to S2SS directory

	$ cd /home/acs-admin/msv/s2ss/server/

4. Edit configuration file

	$ nano etc/opal-test.conf

 - Take a look at the examples and documentation for a detailed description
 - Move with: cursor keys
 - Save with: Ctrl+X => y => Enter

5. Start server

	$ sudo ./server etc/opal-test.conf

 - `sudo` starts the server with super user priviledges

6. Terminate server by pressing Ctrl+C

7. Logout

	$ exit

## Examples

 1. Start server:

	$ ./server etc/example.conf

 2. Receive/dump data to file

	$ ./receive *:10200 > dump.csv

 3. Replay recorded data:

	$ ./send 4 192.168.1.12:10200 < dump.csv

 4. Send random generated values:

	$ ./random 1 4 100 | ./send 4 192.168.1.12:10200

 5. Test ping/pong latency:

	$ ./test latency 192.168.1.12:10200
