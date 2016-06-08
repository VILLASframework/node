#!/usr/bin/env python

import sys

import villas
from villas import *

def main():
	if len(sys.argv) != 3:
		print "Usage: %s from to" % (sys.argv[0])
		sys.exit(-1)

	start = villas.Timestamp.parse(sys.argv[1])
	end = villas.Timestamp.parse(sys.argv[2])

	for line in sys.stdin:
		msg = villas.Message.parse(line)

		if start <= msg.ts <= end:
			print msg

if __name__ == "__main__":
	main()
