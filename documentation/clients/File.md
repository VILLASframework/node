# File

The `file` node-type can be used to log or replay sample values to disk.

## Configuration

Every `file` node supports the following settings:

`in` specifies the path to a file which contains data for replaying.

`out` specifies the path to a file where samples will be dumped.

`mode`

`rate`

### Example

@todo Add extract of example.conf

## File Format

This node-type uses a simple human-readable format to save samples:
The format is similiar to a conventional CSV (comma seperated values) file.
Every line in a file correspondents to a message / sample of simulation data.
The columns of a line are seperated by whitespaces (tabs or spaces).
The columns are defined as follows:

 1. Floating point timestamp in seconds since 1970-01-01 00:00:00 (aka Unix Epoch ).
 2. Up to `MSG_VALUES` floating point values per sample. The values are seperated by whitespaces as well.

### Example

@todo Add example dump
