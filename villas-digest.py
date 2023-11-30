#!/usr/bin/env python
import hashlib
import json
import sys
from ctypes import c_double, c_float, sizeof
from dataclasses import dataclass
from enum import Enum, auto


assert sizeof(c_float) == 4
assert sizeof(c_double) == 8


@dataclass
class Timestamp:
    sec: int
    nsec: int

    def as_bytes(self) -> bytes:
        sec = self.sec.to_bytes(8, "little")
        nsec = self.nsec.to_bytes(8, "little")
        return bytes().join([sec, nsec])


@dataclass
class Sequence:
    number: int

    def as_bytes(self) -> bytes:
        return self.number.to_bytes(8, "little")


class SignalType(Enum):
    Boolean = auto()
    Integer = auto()
    Float = auto()
    Complex = auto()
    Invalid = auto()


SignalData = bool | int | float | complex


@dataclass
class Signal:
    data: SignalData

    @property
    def type(self) -> SignalType:
        match self.data:
            case bool(_):
                return SignalType.Boolean
            case int(_):
                return SignalType.Integer
            case float(_):
                return SignalType.Float
            case complex(_):
                return SignalType.Complex

    def as_bytes(self, byteorder: str) -> bytes:
        match self.data:
            case bool(b):
                return b.to_bytes(1, "little")
            case int(i):
                return i.to_bytes(8, "little")
            case float(f):
                i = int.from_bytes(bytes(c_double(f)), sys.byteorder)
                return i.to_bytes(8, "little")
            case complex(z):
                i_real = int.from_bytes(bytes(c_float(z.real)), sys.byteorder)
                i_imag = int.from_bytes(bytes(c_float(z.imag)), sys.byteorder)
                real = i_real.to_bytes(4, "little")
                imag = i_imag.to_bytes(4, "little")
                return bytes().join([real, imag])


@dataclass
class Sample:
    timestamp: Timestamp
    sequence: Sequence
    signals: list[Signal]
    new_frame: bool

    def parse(sample: dict) -> "Sample":
        ts = sample["ts"]["origin"]

        timestamp = Timestamp(sec=ts[0], nsec=ts[1])
        sequence = Sequence(sample["sequence"])
        signals = [Signal(s) for s in sample["data"]]
        new_frame = "flags" in sample and "new_frame" in sample["flags"]

        return Sample(timestamp, sequence, signals, new_frame)

    def as_bytes(self) -> bytes:
        return bytes().join(
            [
                self.timestamp.as_bytes(),
                self.sequence.as_bytes(),
                *(signal.as_bytes() for signal in self.signals),
            ]
        )


@dataclass
class Frame:
    samples: list[Sample]

    def digest(self, algorithm: str) -> bytes:
        hash = hashlib.new(algorithm)

        for sample in self.samples:
            b = sample.as_bytes()
            hash.update(b)

        return hash.digest()

    def group(samples: list[Sample]) -> list["Frame"]:
        samples.sort()

        if not samples:
            return []

        frames = []
        current_frame = Frame([samples[0]])
        for sample in samples[1:]:
            if sample.new_frame:
                frames.append(current_frame)
                current_frame = Frame([sample])
            else:
                current_frame.samples.append(sample)
        frames.append(current_frame)

        return frames


def main():
    with open("signals.json", "r") as file:
        samples = [Sample.parse(json.loads(s)) for s in file.readlines()]

    frames = Frame.split(samples)
    algorithm = "sha256"

    print("BEGIN FRAMES")
    for frame in frames:
        first = frame.samples[0]
        last = frame.samples[-1]
        first_ts = first.timestamp
        last_ts = last.timestamp
        print("\t", end="")
        print(
            f"{first_ts.sec}.{first_ts.nsec:09}-{first.sequence.number}",
            f"{last_ts.sec}.{last_ts.nsec:09}-{first.sequence.number}",
            algorithm,
            f"{frame.digest(algorithm).hex().upper()}",
        )
    print("END FRAMES")


if __name__ == "__main__":
    main()
