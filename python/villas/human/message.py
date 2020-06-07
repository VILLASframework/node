import villas.human.timestamp as ts
from functools import total_ordering


@total_ordering
class Message:
    """Parsing a VILLASnode sample from a file (not a UDP package!!)"""

    def __init__(self, ts, values, source=None):
        self.source = source
        self.ts = ts
        self.values = values

    @classmethod
    def parse(cls, line, source=None):
        csv = line.split()

        t = ts.Timestamp.parse(csv[0])
        v = map(float, csv[1:])

        return Message(t, v, source)

    def __str__(self):
        return '%s %s' % (self.ts, " ".join(map(str, self.values)))

    def __eq__(self, other):
        return self.ts == other.ts

    def __lt__(self, other):
        return self.ts < other.ts
