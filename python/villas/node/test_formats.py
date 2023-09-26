"""
Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from cmath import sqrt

from villas.node.formats import SignalList, VillasHuman
from villas.node.sample import Sample, Timestamp


def test_signal_list_repr():
    signal_list = SignalList("21fb2ic")
    assert signal_list == eval(repr(signal_list))


def test_signal_list():
    signal_list = SignalList("1fb2ic")
    assert signal_list == SignalList([float, bool, int, int, complex])


def test_villas_human_repr():
    villas_human = VillasHuman(ts_received=False)
    assert villas_human == eval(repr(villas_human))


def test_villas_human():
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
        data=[1.0, 2.0, 3.0, False, 42, sqrt(complex(-1))],
    )

    villas_human = VillasHuman(signal_list=SignalList(smp1))
    smp1_str = "123456780+1.0(4)F\t1.0\t2.0\t3.0\t1\t42\t0.0+1.0i\n"
    smp2_str = "123456789+1.0(5)\t1.0\t2.0\t3.0\t0\t42\t0.0+1.0i\n"
    assert villas_human.dump_sample(smp1) == smp1_str
    assert villas_human.dump_sample(smp2) == smp2_str
    assert villas_human.dumps([smp1, smp2]) == smp1_str + smp2_str
    assert villas_human.load_sample(smp1_str) == smp1
    assert villas_human.load_sample(smp2_str) == smp2
    assert villas_human.loads(smp1_str + smp2_str) == [smp1, smp2]
