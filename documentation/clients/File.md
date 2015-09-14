# File {#file}

The `file` node-type can be used to log or replay samples to / from disk.

## Configuration

Every `file` node supports the following settings:

#### `in` *(string: filesystem path)*

Specifies the path to a file which contains data for replaying.
See below for a description of the file format.

#### `out` *(string: filesystem path)*

Specifies the path to a file where samples will be written to.
This setting allows to add special paceholders for time and date values.
See [strftime(3)](http://man7.org/linux/man-pages/man3/strftime.3.html) for a list of supported placeholder.

**Example**:

    out = "logs/measurements_%Y-%m-%d_%H-%M-%S.log"

will create a file called: *path_of_working_directory*/logs/measurements_2015-08-09_22-20-50.log

See below for a description of the file format.

#### `file_mode` *(string)*

Specifies the mode which should be used to open the output file.
See [open(2)](http://man7.org/linux/man-pages/man2/open.2.html) for an explanation of allowed values.
The default value is `w+` which will start writing at the beginning of the file and create it in case it does not exist yet.

#### `epoch_mode` *("now"|"relative"|"absolute")*

This setting allows to select the behaviour of the following `epoch` setting.
It can be used to adjust the point in time when the first value should be read.

The behaviour of `epoch` is depending on the value of `epoch_mode`.

 - `epoch_mode = now`: The first value is read at *now* + `epoch` seconds.
 - `epoch_mode = relative`: The first value is read at *start* + `epoch` seconds.
 - `epoch_mode = absolute`: The first value is read at `epoch` seconds after 1970-01-01 00:00:00.

#### `send_rate` *(float)*

By default `send_rate` has the value `0` which means that the time between consecutive samples is the same as in the `in` file based on the timestamps in the first column. 

If this setting has a non-zero value, the default behaviour is overwritten with a fixed rate.

### Example

	file_node = {
		type	= "file",
		
	### The following settings are specific to the file node-type!! ###
		mode	= "w+",		# The mode in which files should be opened (see open(2))
							# You might want to use "a+" to append to a file
		
		in	= "logs/file_input.log",	# These options specify the path prefix where the the files are stored
		out	= "logs/file_output_%F_%T.log"	# The output path accepts all format tokens of (see strftime(3))

		epoch_mode = "now"			# One of:
							#  now 		(default)
							#  relative
							#  absolute

		epoch = 10			# The interpretation of this value depends on epoch_mode (default is 0):
							#  - epoch_mode = now:      The first value is read at: _now_ + epoch seconds.
							#  - epoch_mode = relative: The first value is read at _start_ + `epoch` seconds.
							#  - epoch_mode = absolute: The first value is read at epoch seconds after 1970-01-01 00:00:00.
		
		rate	= 2.0		# A constant rate at which the lines of the input files should be read
							# A missing or zero value will use the timestamp in the first column
							# of the file to determine the pause between consecutive lines.
	}

## File Format

This node-type uses a simple human-readable format to save samples:
The format is similiar to a conventional CSV (comma seperated values) file.
Every line in a file correspondents to a message / sample of simulation data.
The columns of a line are seperated by whitespaces (tabs or spaces).
The columns are defined as follows:

 1. Seconds in floating point format since 1970-01-01 00:00:00 (aka Unix epoch timestamp: `date +%s`).
 2. Sequence number
 3. Up to `MSG_VALUES` floating point values per sample. The values are seperated by whitespaces as well.

### Example

This example shows a dump with three values per sample:

    1438959964.162102394	6	3.489760	-1.882725	0.860070
    1438959964.261677582	7	2.375948	-2.204084	0.907518
    1438959964.361622787	8	3.620115	-1.359236	-0.622333
    1438959964.461907066	9	5.844254	-0.966527	-0.628751
    1438959964.561499526	10	6.317059	-1.716363	0.351925
    1438959964.661578339	11	6.471288	-0.159862	0.123948
    1438959964.761956859	12	7.365932	-1.488268	-0.780568
