# Python tools

These python scripts are intended to manipulate measurements recorded by VILLASnode's `file` node-type.

## Exampples

##### Merge two files and filter the output based on timestamps

```
./file-merge.py testfile.dat testfile2.dat | ./file-filter.py 3 5 > output.dat
```
