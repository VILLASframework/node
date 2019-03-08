import csv
import sys
import re

# check if called correctly
if len(sys.argv) != 2:
	sys.exit('Usage: %s FILE' % sys.argv[0])

prog = re.compile('(\d+)(?:\.(\d+))?([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?')

with open(sys.argv[1]) as f:
	reader = csv.reader(f, delimiter='\t')

	for row in reader:
		m = prog.match(row[0])

		if m == None or m.group(3) == None:
			 continue

		offset = float(m.group(3))

		print(offset)
