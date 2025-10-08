"""
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import re
from dataclasses import dataclass, field
from itertools import groupby
from typing import Iterable

from villas.node.sample import Sample, Signal, Timestamp
import villas.node.villas_pb2 as pb


class SignalList(list[type]):
    types = {
        "b": bool,
        "i": int,
        "f": float,
        "c": complex,
    }

    _type_to_char = {t: c for c, t in types.items()}

    def __init__(self, fmt: str | Sample | Iterable[type] = "64f"):
        if isinstance(fmt, Sample):
            super().__init__(map(type, fmt.data) if fmt.data else [])
            return
        elif not isinstance(fmt, str):
            super().__init__(fmt)
            return

        super().__init__()
        regex = f"([{''.join(self.__class__.types.keys())}])"
        fields = iter(re.split(regex, fmt))
        while (count_str := next(fields, None)) is not None:
            if count_str:
                count = int(count_str)
            else:
                count = 1

            try:
                ty_str = next(fields)
            except StopIteration:
                if count_str:
                    raise ValueError("Expected type specifier.")
                else:
                    break

            try:
                ty = self.__class__.types[ty_str]
            except KeyError:
                raise ValueError(f"Unknown type {ty_str}")

            self.extend([ty] * count)

    def __str__(self):
        fmt = ""

        for ty, run in groupby(self):
            run_length = sum(1 for _ in run)
            c = self.__class__._type_to_char[ty]
            if run_length > 1:
                fmt += f"{run_length}"
            fmt += f"{c}"

        return fmt

    def __repr__(self):
        return f"{self.__class__.__name__}('{self.__str__()}')"


@dataclass(kw_only=True)
class Format:
    """
    The base for VILLASnode formats in Python.
    """

    ts_origin: bool = True
    ts_received: bool = True
    sequence: bool = True
    data: bool = True

    def _strip_sample(self, sample: Sample) -> Sample:
        if not self.ts_origin:
            sample.ts_origin = None

        if not self.ts_received:
            sample.ts_received = None

        if not self.sequence:
            sample.sequence = None

        if not self.data:
            sample.data = []

        return sample


@dataclass
class VillasHuman(Format):
    """
    The villas.human format in Python.
    """

    signal_list: SignalList = field(default_factory=SignalList)
    separator: str = "\t"
    delimiter: str = "\n"

    def load(self, file) -> list[Sample]:
        """
        Load samples from a text mode file object.
        """

        return self.loads(file.read())

    def loads(self, s: str) -> list[Sample]:
        """
        Load samples from a string.
        """

        s.strip(self.separator + self.delimiter)
        sample_strs = s.split(sep=self.delimiter)
        samples = (self.load_sample(sample) for sample in sample_strs)
        return [s for s in samples if s is not None]

    def dump(self, samples: Iterable[Sample], file):
        """
        Dump samples to a text mode file object.
        """

        return file.write(self.dumps(samples))

    def dumps(self, samples: Iterable[Sample]) -> str:
        """
        Dump samples to a string.
        """

        sample_strs = (self.dump_sample(sample) for sample in iter(samples))
        return "".join(sample_strs)

    def load_sample(self, sample: str) -> Sample | None:
        """
        Load a single sample from a string.
        """

        sample = sample.strip(self.delimiter)

        if sample.startswith("#"):
            return None

        fields = sample.split(sep=self.separator)

        if not fields[0]:
            return None

        m = re.match(
            r"(\d+)(?:\.(\d+))?([-+]\d+(?:\.\d+)?"
            r"(?:e[+-]?\d+)?)?(?:\((\d+)\))?(F)?",
            fields[0],
        )

        if m is None:
            raise ValueError(f"Invalid header: {fields[0]}")

        ts_seconds = int(m.group(1))
        ts_nanoseconds = int(m.group(2)) if m.group(2) else 0
        ts_offset = float(m.group(3)) if m.group(3) else None
        sequence = int(m.group(4)) if m.group(4) else None
        new_frame = bool(m.group(5))

        ts_origin = Timestamp(ts_seconds, ts_nanoseconds)
        if ts_offset is not None:
            ts_received_raw = ts_origin.timestamp() + ts_offset
            ts_received = Timestamp.fromtimestamp(ts_received_raw)
        else:
            ts_received = None

        data: list[Signal] = []
        for ty, value in zip(self.signal_list, fields[1:]):
            if ty is bool:
                data.append(bool(int(value)))
            elif ty is int:
                data.append(int(value))
            elif ty is float:
                data.append(float(value))
            elif ty is complex:
                data.append(self._unpack_complex(value))

        return self._strip_sample(
            Sample(
                ts_origin=ts_origin,
                ts_received=ts_received,
                sequence=sequence,
                new_frame=new_frame,
                data=data,
            )
        )

    def dump_sample(self, smp: Sample) -> str:
        """
        Dump a single sample to a string.
        """

        smp = self._strip_sample(smp)

        s = ""
        if smp.ts_origin is not None:
            s += f"{smp.ts_origin.seconds}"
            if smp.ts_origin.nanoseconds != 0:
                s += f".{smp.ts_origin.nanoseconds:09}"
            if smp.ts_received is not None:
                off = smp.ts_received.timestamp() - smp.ts_origin.timestamp()
                s += f"+{off}"
        if smp.sequence is not None:
            s += f"({smp.sequence})"
        if smp.new_frame:
            s += "F"

        for ty, value in zip(self.signal_list, smp.data):
            s += self.separator
            assert ty == type(value)
            match value:
                case bool():
                    s += str(int(value))
                case int():
                    s += str(value)
                case float():
                    s += str(value)
                case complex():
                    s += self._pack_complex(value)

        s += self.delimiter

        return s

    def _unpack_complex(self, s: str) -> complex:
        return complex(s.lower().replace("i", "j"))

    def _pack_complex(self, z: complex) -> str:
        return f"{z.real}+{z.imag}i"


@dataclass
class Protobuf(Format):
    """
    The protobuf format in Python.
    """

    def loadb(self, b: bytes) -> list[Sample]:
        msg = pb.Message()
        msg.ParseFromString(b)

        return [self.load_sample(sample) for sample in msg.samples]

    def dumpb(self, samples: Iterable[Sample]) -> bytes:
        msg = pb.Message()

        for sample in samples:
            msg.samples.append(self.dump_sample(sample))

        return msg.SerializeToString()

    def load_sample(self, pb_sample: pb.Sample) -> Sample:
        sample = Sample()

        if pb_sample.HasField("ts_origin"):
            sample.ts_origin = Timestamp(
                pb_sample.ts_origin.sec, pb_sample.ts_origin.nsec
            )

        if pb_sample.HasField("ts_received"):
            sample.ts_received = Timestamp(
                pb_sample.ts_received.sec, pb_sample.ts_received.nsec
            )

        if pb_sample.HasField("new_frame"):
            sample.new_frame = pb_sample.new_frame

        if pb_sample.HasField("sequence"):
            sample.sequence = pb_sample.sequence

        for value in pb_sample.values:
            if value.HasField("i"):
                sample.data.append(value.i)
            elif value.HasField("f"):
                sample.data.append(value.f)
            elif value.HasField("b"):
                sample.data.append(value.b)
            elif value.HasField("z"):
                sample.data.append(complex(value.z.real, value.z.imag))
            else:
                raise Exception("Missing value")

        return self._strip_sample(sample)

    def dump_sample(self, sample: Sample) -> pb.Sample:
        pb_sample = pb.Sample()
        pb_sample.type = pb.Sample.Type.DATA

        pb_sample.new_frame = sample.new_frame

        if sample.ts_origin:
            pb_sample.ts_origin.sec = sample.ts_origin.seconds
            pb_sample.ts_origin.nsec = sample.ts_origin.nanoseconds

        if sample.ts_received:
            pb_sample.ts_received.sec = sample.ts_received.seconds
            pb_sample.ts_received.nsec = sample.ts_received.nanoseconds

        if sample.sequence:
            pb_sample.sequence = sample.sequence

        for value in sample.data:
            pb_value = pb.Value()

            if isinstance(value, bool):
                pb_value.b = value
            elif isinstance(value, float):
                pb_value.f = value
            elif isinstance(value, int):
                pb_value.i = value
            elif isinstance(value, complex):
                pb_value.z.real = value.real
                pb_value.z.imag = value.imag

            pb_sample.values.append(pb_value)

        return pb_sample
