#!/usr/bin/env python3

import sys
import matplotlib as mpl
import numpy as np
import matplotlib.pyplot as plt

def read_datafile(file_name):
    # the skiprows keyword is for heading, but I don't know if trailing lines
    # can be specified
    data = np.loadtxt(file_name, delimiter='\t', skiprows=1)
    return data

filename = sys.argv[1]

data = read_datafile(filename)

print(data[:,0])

fig, ax1 = plt.subplots()
ax1.plot(data[:,0], data[:,1])

ax2 = ax1.twinx()
ax2.plot(data[:,0], data[:,2], c='red')

fig.tight_layout()
plt.show()
