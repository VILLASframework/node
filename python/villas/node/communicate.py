"""
Author: Steffen Vogel <post@steffenvogel.de>
SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import logging
import sys
from threading import Thread
from typing import Callable

import linuxfd  # type: ignore[import]
from villas.node.formats import VillasHuman
from villas.node.sample import Sample

logger = logging.getLogger(__name__)


RecvCallback = Callable[[Sample], None]
SendCallback = Callable[[int], Sample]


class RecvThread(Thread):
    def __init__(self, cb: RecvCallback):
        super().__init__()
        self.cb = cb
        self.daemon = True
        self.format = VillasHuman()

    def run(self):
        for line in sys.stdin:
            logger.debug(f"RecvThread: {line}")

            if (sample := self.format.load_sample(line)) is not None:
                self.cb(sample)


class SendThread(Thread):
    def __init__(self, cb: SendCallback, rate: float):
        super().__init__()
        self.cb = cb
        self.daemon = True
        self.format = VillasHuman()
        self.rate = rate
        self.sequence = 0

    def run(self):
        tfd = linuxfd.timerfd()
        tfd.settime(1.0, 1.0 / self.rate)

        while True:
            tfd.read()

            sample = self.cb(self.sequence)
            if sample is None:
                continue

            sample = self.format.dump_sample(sample)
            sys.stdout.write(sample)
            sys.stdout.flush()
            self.sequence += 1


def communicate(
    rate: float,
    recv_cb: RecvCallback | None = None,
    send_cb: SendCallback | None = None,
    wait: bool = True,
):
    if recv_cb is not None:
        rt = RecvThread(recv_cb)
        rt.start()

    if send_cb is not None:
        st = SendThread(send_cb, rate)
        st.start()

    if wait:
        try:
            rt.join()
            st.join()
        except KeyboardInterrupt:
            logger.info("Received Ctrl+C. Stopping send/recv threads")
