import re
from functools import total_ordering


@total_ordering
class Timestamp:
    """Parsing the VILLASnode human-readable timestamp format"""

    def __init__(self, seconds=0, nanoseconds=None,
                 offset=None, sequence=None):
        self.seconds = seconds
        self.nanoseconds = nanoseconds
        self.offset = offset
        self.sequence = sequence

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

    def __eg__(self, other):
        return float(self) == float(other)

    def __lt__(self, other):
        return float(self) < float(other)
