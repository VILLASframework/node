"""
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from cmath import sqrt

from villas.node.digest import Digest, Frame
from villas.node.sample import Sample, Timestamp


def test_frame_repr():
    smp1 = Sample(
        ts_origin=Timestamp(123456780),
        ts_received=Timestamp(123456781),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp2 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=5,
        new_frame=False,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    frame = Frame([smp1, smp2])
    assert frame == eval(repr(frame))


def test_frame_group():
    smp1 = Sample(
        ts_origin=Timestamp(123456780),
        ts_received=Timestamp(123456781),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp2 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=5,
        new_frame=False,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp3 = Sample(
        ts_origin=Timestamp(123456791),
        ts_received=Timestamp(123456793),
        sequence=6,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    frames = list(Frame.group([smp1, smp2, smp3]))
    assert len(frames) == 2
    assert list(map(len, frames)) == [2, 1]
    assert frames == [[smp1, smp2], [smp3]]


def test_digest_fromframe():
    smp1 = Sample(
        ts_origin=Timestamp(123456780),
        ts_received=Timestamp(123456781),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp2 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=5,
        new_frame=False,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    algorithm = "sha256"

    digest_hex = (
        "a573e3b0953a1e4f69addf631d6229bb714d263b4f362f0847e96c3838c83217"  # noqa: E501
    )

    digest = Digest(
        first=(Timestamp(123456780), 4),
        last=(Timestamp(123456789), 5),
        algorithm=algorithm,
        bytes=bytes.fromhex(digest_hex),
    )

    assert Frame([smp1, smp2]).digest(algorithm) == digest


def test_digest():
    digest_str = "1695904705.856457323-0 1695904709.956060462-41 sha256 8E49482DDDAFF6E3B7411D7D20CA338002126FE109EC1BA5932C02FC5E7EFD23"  # noqa: E501

    digest = Digest(
        first=(Timestamp(1695904705, 856457323), 0),
        last=(Timestamp(1695904709, 956060462), 41),
        algorithm="sha256",
        bytes=bytes.fromhex(
            "8E49482DDDAFF6E3B7411D7D20CA338002126FE109EC1BA5932C02FC5E7EFD23"
        ),  # noqa: E501
    )

    assert Digest.parse(digest_str) == digest
    assert digest_str == digest.dump()
