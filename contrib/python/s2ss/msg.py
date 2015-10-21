from . import ts

class Message:
	"""Parsing a S2SS from a file (not a UDP package!!)"""

	def __init__(self, ts, values, source = None):
		self.source = source
		self.ts = ts
		self.values = values

	@classmethod
	def parse(self, line, source = None):
		csv = line.split()
	
		t = ts.Timestamp.parse(csv[0])
		v = map(float, csv[1:])
		
		return Message(t, v, source)

	def __str__(self):
		return '%s %s' % (self.ts, " ".join(map(str, self.values)))

	def __cmp__(self, other):
		return cmp(self.ts, other.ts)
