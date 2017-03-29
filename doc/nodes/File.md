# File {#file}

The `file` node-type can be used to log or replay samples to / from disk.

## Configuration

Every `file` node can be configured to only read or write or to do both at the same time.
The node configuration is splitted in to groups: `in` and `out`.

#### `uri` *(string: libcurl URI)*

Specifies the URI to a file from which is written to or read from depending in which group (`in`or `out`) is used.

See below for a description of the file format.
This setting allows to add special paceholders for time and date values.
See [strftime(3)](http://man7.org/linux/man-pages/man3/strftime.3.html) for a list of supported placeholder.

**Example**:

    uri = "logs/measurements_%Y-%m-%d_%H-%M-%S.log"

will create a file called: *path_of_working_directory*/logs/measurements_2015-08-09_22-20-50.log

See below for a description of the file format.

#### `mode` *(string)*

Specifies the mode which should be used to open the output file.
See [open(2)](http://man7.org/linux/man-pages/man2/open.2.html) for an explanation of allowed values.
The default value is `w+` which will start writing at the beginning of the file and create it in case it does not exist yet.

#### `epoch_mode` *("direct" | "wait" | "relative" | "absolute")*

The *epoch* describes the point in time when the first message will be read from the file.
This setting allows to select the behaviour of the following `epoch` setting.
It can be used to adjust the point in time when the first value should be read.

The behaviour of `epoch` is depending on the value of `epoch_mode`.

To facilitate the following description of supported `epoch_mode`'s, we will introduce some intermediate variables (timestamps).
Those variables will also been displayed during the startup phase of the server to simplify debugging.

- `epoch` is the value of the `epoch` setting.
- `first` is the timestamp of the first message / line in the input file.
- `offset` will be added to the timestamps in the file to obtain the real time when the message will be sent.
- `start` is the point in time when the first message will be sent (`first + offset`).
- `eta` the time to wait until the first message will be send (`start - now`)

The supported values for `epoch_mode`:
 
 | `epoch_mode` | `offset`              | `start = first + offset` |
 | -----------: | :-------------------: | :----------------------: |
 | `direct`     | `now - first + epoch` | `now + epoch`            |
 | `wait`       | `now + epoch`         | `now + first`            | 
 | `relative`   | `epoch`               | `first + epoch`          |
 | `absolute`   | `epoch - first`       | `epoch`                  |

#### `rate` *(float)*

By default `send_rate` has the value `0` which means that the time between consecutive samples is the same as in the `in` file based on the timestamps in the first column. 

If this setting has a non-zero value, the default behaviour is overwritten with a fixed rate.

#### `split` *(integer)*

Only valid for the `out` group.

Splits the output file every `split` mega-byte. This setting will append the chunk number to the `uri` setting.

Example: `data/my_measurements.log_001`

#### `splitted` *(boolean)*

Only valid for the `in` group.

Expects the input data in splitted format.

### Example

	nodes = {
		file_node = {
			type	= "file",
		
		### The following settings are specific to the file node-type!! ###
	
			in = {
				uri = "logs/input.log",	# These options specify the URI where the the files are stored
				mode = "w+",			# The mode in which files should be opened (see open(2))
							
				epoch_mode = "direct"		# One of: direct (default), wait, relative, absolute
				epoch = 10			# The interpretation of this value depends on epoch_mode (default is 0).
								# Consult the documentation of a full explanation
	
				rate = 2.0			# A constant rate at which the lines of the input files should be read
								# A missing or zero value will use the timestamp in the first column
								# of the file to determine the pause between consecutive lines.
			
				splitted = false
			},
			out = {
				URI = "logs/output_%F_%T.log"	# The output URI accepts all format tokens of (see strftime(3))
				mode = "a+"			# You might want to use "a+" to append to a file
	
				split	= 100,			# Split output file every 100 MB
			}
		}
	}

## File Format

This node-type uses a simple human-readable format to save samples to disk:
The format is similiar to a conventional CSV (comma seperated values) file.
Every line in a file correspondents to a message / sample of simulation data.
The columns of a line are seperated by whitespaces (tabs or spaces).
The columns are defined as follows:

    seconds.nanoseconds+offset(sequenceno)	value0 value1 ... valueN

 1. The first column contains a timestamp which is composed of up to 4 parts:
     - Number of seconds after 1970-01-01 00:00:00 UTC
     - A dot: '.'
     - Number of nano seconds of the current second (optional)
     - An offset between the point in time when a message was sent and received (optional)
     - The sequence number of the message (optional)
     
     A valid timestamp can be generated by the following Unix command: `date +%s.%N`.
     *Important:* The second field is not the fractional part of the second!!!

 2. Maximum `MSG_VALUES` floating point values per sample. The values are seperated by whitespaces as well.

### Example

This example shows a dump with three values per sample:

    1438959964.162102394	6	3.489760	-1.882725	0.860070
    1438959964.261677582	7	2.375948	-2.204084	0.907518
    1438959964.361622787	8	3.620115	-1.359236	-0.622333
    1438959964.461907066	9	5.844254	-0.966527	-0.628751
    1438959964.561499526	10	6.317059	-1.716363	0.351925
    1438959964.661578339	11	6.471288	-0.159862	0.123948
    1438959964.761956859	12	7.365932	-1.488268	-0.780568
