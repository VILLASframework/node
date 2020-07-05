import re
from datetime import datetime
from functools import total_ordering


@total_ordering
class Timestamp:
    """Parsing the VILLASnode human-readable timestamp format"""

    def __init__(self, seconds=None, nanoseconds=None,
                 offset=None, sequence=None):
        self.seconds = seconds
        self.nanoseconds = nanoseconds
        self.offset = offset
        self.sequence = sequence

    @classmethod
    def now(cls, offset=None, sequence=None):
        n = datetime.utcnow()

        secs = int(n.timestamp())
        nsecs = 1000 * n.microsecond

        return Timestamp(seconds=secs, nanoseconds=nsecs,
                         offset=offset, sequence=sequence)

    @classmethod
    def parse(cls, ts):
        m = re.match(r'(\d+)(?:\.(\d+))?([-+]\d+(?:\.\d+)?'
                     r'(?:e[+-]?\d+)?)?(?:\((\d+)\))?', ts)

        seconds = int(m.group(1))  # Mandatory
        nanoseconds = int(m.group(2)) if m.group(2) else None
        offset = float(m.group(3)) if m.group(3) else None
        sequence = int(m.group(4)) if m.group(4) else None

        return Timestamp(seconds, nanoseconds, offset, sequence)

    def __str__(self):
        str = "%u" % (self.seconds)

        if self.nanoseconds is not None:
            str += ".%09u" % self.nanoseconds
        if self.offset is not None:
            str += "+%u" % self.offset
        if self.sequence is not None:
            str += "(%u)" % self.sequence

        return str

    def __float__(self):
        sum = float(self.seconds)

        if self.nanoseconds is not None:
            sum += self.nanoseconds * 1e-9
        if self.offset is not None:
            sum += self.offset

        return sum

    def __eq__(self, other):
        return float(self) == float(other)

    def __lt__(self, other):
        return float(self) < float(other)


@total_ordering
class Sample:
    """Parsing a VILLASnode sample from a file (not a UDP package!!)"""

    def __init__(self, ts, values):
        self.ts = ts
        self.values = values

    @classmethod
    def parse(cls, line):
        csv = line.split()

        ts = Timestamp.parse(csv[0])
        vs = []

        for value in csv[1:]:
            try:
                v = float(value)
            except ValueError:
                value = value.lower()
                try:
                    v = complex(value)
                except Exception:
                    if value.endswith('i'):
                        v = complex(value.replace('i', 'j'))
                    else:
                        raise ValueError()

            vs.append(v)

        return Sample(ts, vs)

    def __str__(self):
        return '%s\t%s' % (self.ts, "\t".join(map(str, self.values)))

    def __eq__(self, other):
        return self.ts == other.ts

    def __lt__(self, other):
        return self.ts < other.ts
