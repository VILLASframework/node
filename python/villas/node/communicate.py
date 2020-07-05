import time
import logging
import sys
import linuxfd
from villas.node.sample import Sample, Timestamp
from threading import Thread

logger = logging.getLogger(__name__)


class RecvThread(Thread):

    def __init__(self, cb):
        super().__init__()

        self.cb = cb
        self.daemon = True

    def run(self):
        for line in sys.stdin:
            if line.startswith('#'):
                continue

            logger.debug("RecvThread: {}".format(line))

            sample = Sample.parse(line)

            self.cb(sample.values)


class SendThread(Thread):

    def __init__(self, cb, rate=None):
        super().__init__()

        self.cb = cb
        self.rate = rate
        self.daemon = True

        self.sequence = 0

    def run(self):

        if self.rate:
            tfd = linuxfd.timerfd()
            tfd.settime(1.0, 1.0 / self.rate)
        else:
            tfd = None

        while True:
            if tfd:
                tfd.read()

            values = self.cb()
            ts = Timestamp.now(None, self.sequence)

            sample = Sample(ts, values)

            sys.stdout.write(str(sample) + '\n')
            sys.stdout.flush()

            self.sequence += 1


def communicate(rate, recv_cb=None, send_cb=None, wait=True):

    if recv_cb:
        rt = RecvThread(recv_cb)
        rt.start()

    if send_cb:
        st = SendThread(send_cb, rate)
        st.start()

    if wait:
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info('Received Ctrl+C. Stopping send/recv threads')

    # Threads are daemon threads
    # and therefore killed with program termination
