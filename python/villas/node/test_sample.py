"""
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from cmath import sqrt
from datetime import datetime, timezone

from villas.node.sample import Sample, Timestamp


def test_timestamp_repr():
    ts = Timestamp(123456789, 123456789)
    assert ts == eval(repr(ts))


def test_timestamp_conversion():
    ts = Timestamp(123456789, 123456789)

    fl = 123456789.123456789
    fl_ts = Timestamp(123456789, 123456791)
    assert ts.timestamp() == fl
    assert fl_ts == Timestamp.fromtimestamp(fl)

    dt = datetime(1973, 11, 29, 21, 33, 9, 123457, tzinfo=timezone.utc)
    dt_ts = Timestamp(123456789, 123457000)
    assert ts.datetime() == dt
    assert dt_ts == Timestamp.fromdatetime(dt)


def test_timestamp_ordering():
    ts1 = Timestamp(123456789)
    ts2 = Timestamp(123456789, 0)
    ts3 = Timestamp(123456789, 123456789)
    assert ts1 == ts2
    assert ts2 < ts3


def test_timestamp_as_digest_bytes():
    ts = Timestamp(123456789, 123456789)
    digest_bytes = bytes.fromhex("15cd5b070000000015cd5b0700000000")
    assert ts._as_digest_bytes() == digest_bytes


def test_sample_repr():
    smp = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )
    assert smp == eval(repr(smp))


def test_sample_ordering():
    smp1 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp2 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    smp3 = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456791),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    assert smp1 == smp2
    assert smp2 < smp3


def test_sample_as_digest_bytes():
    smp = Sample(
        ts_origin=Timestamp(123456789),
        ts_received=Timestamp(123456790),
        sequence=4,
        new_frame=True,
        data=[1.0, 2.0, 3.0, True, 42, sqrt(complex(-1))],
    )

    digest_bytes_hex = "15cd5b070000000000000000000000000400000000000000000000000000f03f00000000000000400000000000000840012a00000000000000000000000000803f"  # noqa: E501
    digest_bytes = bytes.fromhex(digest_bytes_hex)
    assert smp._as_digest_bytes() == digest_bytes, smp._as_digest_bytes().hex()
