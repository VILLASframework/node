# Usage {#usage}

The S2SS server (`s2ss server`) expects the path to a configuration file as a single argument.

    Usage: ./s2ss-server CONFIG
      CONFIG is a required path to a configuration file
     
    Simulator2Simulator Server 0.1-d7de19c (Jun  4 2014 02:50:13)
      Copyright 2015, Institute for Automation of Complex Power Systems, EONERC
        Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

The server requires root privileges for:

 - Enable the realtime FIFO scheduler
 - Increase the task priority
 - Configure the network emulator (netem)
 - Change the SMP affinity of threads and network interrupts

## Step-by-step

1. Start putty.exe (SSH client for Windows)

  - Should be already installed on most systems
  - Or load .exe from this website (no installation required)
	    http://the.earth.li/~sgtatham/putty/latest/x86/putty.exe

2. Connect to S2SS server

| Setting  | Value           |
| :------- | :-------------- |
| IP       | 130.134.169.31  |
| Port     | 22              |
| Protocol | SSH             |
| User     | root            |
| Password | *please ask msv |

3. Go to S2SS directory

    $ cd /s2ss/

4. Edit configuration file

    $ nano etc/opal-test.conf

 - Take a look at the examples and documentation for a detailed description
 - Move with: cursor keys
 - Save with: Ctrl+X => y => Enter

5. Start server

    $ s2ss server etc/opal-test.conf

6. Terminate server by pressing Ctrl+C

7. Logout

    $ exit
