"""
Author: Steffen Vogel <post@steffenvogel.de>
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from ctypes import c_double, c_float, sizeof
from dataclasses import dataclass, field
from datetime import datetime, timezone
from functools import total_ordering
from sys import byteorder as native

assert sizeof(c_float) == 4
assert sizeof(c_double) == 8


Signal = bool | int | float | complex


@total_ordering
@dataclass
class Timestamp:
    """
    A VILLASnode timestamp. Based on the C struct timespec.
    These timestamps are always UTC.
    """

    seconds: int
    nanoseconds: int = 0

    def _as_digest_bytes(self):
        sec = self.seconds.to_bytes(8, "little")
        nsec = self.nanoseconds.to_bytes(8, "little")
        return bytes().join([sec, nsec])

    @classmethod
    def fromdatetime(cls, ts: datetime) -> "Timestamp":
        secs = int(ts.timestamp())
        nsecs = int(1000 * ts.microsecond)
        return cls(seconds=secs, nanoseconds=nsecs)

    @classmethod
    def fromtimestamp(cls, ts: float) -> "Timestamp":
        secs = int(ts)
        nsecs = int(1e9 * (ts - float(secs)))
        return cls(seconds=secs, nanoseconds=nsecs)

    def timestamp(self) -> float:
        return float(self)

    def datetime(self) -> datetime:
        return datetime.fromtimestamp(self.timestamp(), tz=timezone.utc)

    def __float__(self):
        return float(self.seconds) + float(self.nanoseconds) * 1e-9

    def _as_ordered_tuple(self):
        return (
            self.seconds,
            self.nanoseconds,
        )

    def __eq__(self, other: object):
        if not isinstance(other, Timestamp):
            return False

        return self._as_ordered_tuple() == other._as_ordered_tuple()

    def __lt__(self, other: "Timestamp"):
        return self._as_ordered_tuple() < other._as_ordered_tuple()


@total_ordering
@dataclass(kw_only=True)
class Sample:
    """
    A VILLASnode sample.
    """

    ts_origin: Timestamp | None = None
    ts_received: Timestamp | None = None
    sequence: int | None = None
    new_frame: bool = False
    data: list[Signal] = field(default_factory=list)

    def _as_ordered_tuple(self):
        return (
            self.ts_origin is not None,
            self.ts_origin if self.ts_origin is not None else Timestamp(0),
            self.ts_received is not None,
            self.ts_received if self.ts_received is not None else Timestamp(0),
            self.sequence is not None,
            self.sequence if self.sequence is not None else 0,
            not self.new_frame,
            self.data,
        )

    def __eq__(self, other: object):
        if not isinstance(other, Sample):
            return False

        return self._as_ordered_tuple() == other._as_ordered_tuple()

    def __lt__(self, other: "Timestamp"):
        return self._as_ordered_tuple() < other._as_ordered_tuple()

    def _as_digest_bytes(self):
        def signal_to_bytes(signal):
            match signal:
                case bool():
                    return signal.to_bytes(1, "little")
                case int():
                    return signal.to_bytes(8, "little")
                case float():
                    i = int.from_bytes(bytes(c_double(signal)), native)
                    return i.to_bytes(8, "little")
                case complex():
                    f_real = signal.real
                    f_imag = signal.imag
                    i_real = int.from_bytes(bytes(c_float(f_real)), native)
                    i_imag = int.from_bytes(bytes(c_float(f_imag)), native)
                    real = i_real.to_bytes(4, "little")
                    imag = i_imag.to_bytes(4, "little")
                    return bytes().join([real, imag])

        return bytes().join(
            [
                self.ts_origin._as_digest_bytes(),
                self.sequence.to_bytes(8, "little"),
            ]
            + list(map(signal_to_bytes, self.data))
        )
