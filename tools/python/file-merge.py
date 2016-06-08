#!/usr/bin/env python

import sys

import villas
from villas import *

def main():
	files = sys.argv[1:]
	
	all = [ ]
	last = { }
	
	for file in files:
		handle = sys.stdin if file == '-' else open(file, "r")

		msgs = [ ]
		for line in handle.xreadlines():
			msgs.append(villas.Message.parse(line, file))
		
		all += msgs
		last[file] = villas.Message(villas.Timestamp(), [0] * len(msgs[0].values), file)

	all.sort()
	for msg in all:
		last[msg.source] = msg

		values = [ ]
		for file in files:
			values += last[file].values
			
		print villas.Message(msg.ts, values, "")

if __name__ == "__main__":
	main()
