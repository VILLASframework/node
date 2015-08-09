# File {#file}

The `file` node-type can be used to log or replay sample values to disk.

## Configuration

Every `file` node supports the following settings:

#### `in`

Specifies the path to a file which contains data for replaying.
See below for a description of the file format.

#### `out`

Specifies the path to a file where samples will be written to.
This setting allows to add special paceholders for time and date values.
See [strftime(3)](http://man7.org/linux/man-pages/man3/strftime.3.html) for a list of supported placeholder.

**Example**:

    out = "logs/measurements_%Y-%m-%d_%H-%M-%S.log"

will create a file called: *path_of_working_directory*/logs/measurements_2015-08-09_22-20-50.log

#### `file_mode`

Specifies the mode which should be used to open the output file.
See [open(2)](http://man7.org/linux/man-pages/man2/open.2.html) for an explanation of allowed values.
The default value is `w+` which will start writing at the beginning of the file and create it in case it does not exist yet.

#### `epoch_mode`

This setting allows to select the behaviour of the following `epoch` setting.
It can be used to adjust the point in time when the first value should be read.

The behaviour of `epoch` is depending on the value of `epoch_mode`.

 - `epoch_mode = now`: The first value is read at *now* + `epoch` seconds.
 - `epoch_mode = relative`: The first value is read at *start* + `epoch` seconds.
 - `epoch_mode = absolute`: The first value is read at `epoch` seconds after 1970-01-01 00:00:00.

#### `rate`

By default `rate` has the value `0`. If the value is non-zero,

### Example

@todo Add extract of example.conf

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
