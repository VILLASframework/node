"""
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import hashlib
from dataclasses import dataclass
from typing import Iterable

from villas.node.sample import Sample, Timestamp


@dataclass
class Frame(list[Sample]):
    """
    A frame VILLASnode of sample indicated by the new_frame flag.
    """

    def __init__(self, it: Iterable[Sample]):
        super().__init__(it)

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({super().__repr__()})"

    def _raw_digest(self, algorithm) -> bytes:
        hash = hashlib.new(algorithm)

        for sample in self:
            hash.update(sample._as_digest_bytes())

        return hash.digest()

    def digest(self, algorithm: str) -> "Digest":
        """
        A digest for a frame of samples that is comparable to the digest hook.
        """

        return Digest.fromframe(self, algorithm)

    def group(samples: list[Sample]) -> list["Frame"]:
        """
        Group samples into Frames according to their new_frame flag.
        """

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
                current_frame.append(sample)
        frames.append(current_frame)

        return frames


@dataclass
class Digest:
    first: tuple[Timestamp, int]
    last: tuple[Timestamp, int]
    algorithm: str
    bytes: bytes

    @staticmethod
    def fromframe(frame: Frame, algorithm: str) -> "Digest":
        first = frame[0]
        last = frame[-1]

        def isnone(x):
            return x is None

        if any(map(isnone, [first.ts_origin, last.ts_origin])):
            raise ValueError("Missing origin timestamp for digest.")

        if any(map(isnone, [first.sequence, last.sequence])):
            raise ValueError("Missing sequence number for digest.")

        return Digest(
            first=(first.ts_origin, first.sequence),  # type: ignore[arg-type]
            last=(last.ts_origin, last.sequence),  # type: ignore[arg-type]
            algorithm=algorithm,
            bytes=frame._raw_digest(algorithm),
        )

    @staticmethod
    def _parse_timestamp(s: str) -> tuple[Timestamp, int]:
        ts, seq = s.split("-")
        sec, nsec = ts.split(".")
        return Timestamp(seconds=int(sec), nanoseconds=int(nsec)), int(seq)

    @staticmethod
    def parse(s: str) -> "Digest":
        first, last, algorithm, digest = s.split(" ")
        return Digest(
            first=Digest._parse_timestamp(first),
            last=Digest._parse_timestamp(last),
            algorithm=algorithm,
            bytes=bytes.fromhex(digest),
        )

    @staticmethod
    def _dump_timestamp(timestamp: Timestamp, sequence: int) -> str:
        return f"{timestamp.seconds}.{timestamp.nanoseconds}-{sequence}"

    def dump(self) -> str:
        first = Digest._dump_timestamp(*self.first)
        last = Digest._dump_timestamp(*self.last)
        algorithm = self.algorithm
        digest = self.bytes.hex().upper()
        return f"{first} {last} {algorithm} {digest}"
