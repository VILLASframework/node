# Usage Instructions

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


