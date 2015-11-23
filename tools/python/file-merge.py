#!/usr/bin/env python

import sys

import s2ss
from s2ss import *

def main():
	files = sys.argv[1:]
	
	all = [ ]
	last = { }
	
	for file in files:
		handle = sys.stdin if file == '-' else open(file, "r")

		msgs = [ ]
		for line in handle.xreadlines():
			msgs.append(s2ss.Message.parse(line, file))
		
		all += msgs
		last[file] = s2ss.Message(s2ss.Timestamp(), [0] * len(msgs[0].values), file)

	all.sort()
	for msg in all:
		last[msg.source] = msg

		values = [ ]
		for file in files:
			values += last[file].values
			
		print s2ss.Message(msg.ts, values, "")

if __name__ == "__main__":
	main()
