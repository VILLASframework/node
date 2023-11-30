#!/usr/bin/env python

"""
SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""

from datetime import datetime, timezone
from paho.mqtt import client as mqtt
from rfc3161ng import decode_timestamp_response, TimeStampResp, oid_to_hash, check_timestamp, get_timestamp
from pyasn1.codec import der
from pyasn1.type.univ import OctetString
from subprocess import run
from dataclasses import dataclass
from os import path, set_blocking
from dateutil.parser import parse
from multiprocessing import Process, Queue as MpQueue
from queue import Queue
from threading import Thread
from enum import Enum, auto


RESPONSE_TOLERANCE_SECS = 120


class MessageType(Enum):
    Digest = auto()
    TimeStampResponse = auto()


@dataclass
class Sample:
    tv_sec: int
    tv_nsec: int
    sequence: int

    def from_str(s: str) -> "Sample":
        s = s.strip()
        tv, sequence = s.split("-")
        tv_sec, tv_nsec = tv.split(".")

        return Sample(
            tv_sec=int(tv_sec),
            tv_nsec=int(tv_nsec),
            sequence=int(sequence)
        )


@dataclass
class Digest:
    first: Sample
    last: Sample
    algorithm: str
    hex_str: str

    def from_str(s: str) -> "Digest":
        s = s.strip()
        first, last, algorithm, hex_str = s.split(" ")

        return Digest(
            first=Sample.from_str(first),
            last=Sample.from_str(last),
            algorithm=algorithm,
            hex_str=hex_str
        )


def verify(digest: Digest, timestamp_response: TimeStampResp, certificate):
    """Verify a digest against a tsr file."""

    response_ts = get_timestamp(timestamp_response.time_stamp_token, naive=False)
    digest_ts = datetime.fromtimestamp(digest.last.tv_sec, timezone.utc)

    if response_ts < digest_ts:
        raise ValueError("response was created before digest")

    if (response_ts - digest_ts).total_seconds() > RESPONSE_TOLERANCE_SECS:
        raise ValueError(f"response was created more than {RESPONSE_TOLERANCE_SECS}s after the digest")

    check_timestamp(
        tst=timestamp_response.time_stamp_token,
        digest=OctetString.fromHexString(digest.hex_str),
        hashname=digest.algorithm,
        certificate=certificate,
    )


class FifoWorker:
    def __init__(self, queue: MpQueue, path: str):
        self.__process = Process(target=FifoWorker.__worker, args=(queue, path))

    def __worker(queue: MpQueue, path: str):
        try:
            while True:
                with open(path, "r") as fifo:
                    while message := fifo.readline():
                        digest = Digest.from_str(message)
                        key = f"{digest.algorithm}:{digest.hex_str}"
                        queue.put((MessageType.Digest, key, digest))

        except KeyboardInterrupt:
            pass

    def __enter__(self):
        self.__process.start()

    def __exit__(self, _type, _value, _traceback):
        self.__process.terminate()


class MqttWorker:
    def __init__(self, queue: MpQueue, broker: str, port: int, topic: str):
        self.__client = mqtt.Client()
        self.__client.on_message = MqttWorker.__on_message
        self.__client.user_data_set(queue)
        self.__client.connect(broker, port)
        self.__client.subscribe(topic)

    def __on_message(_, queue, message):
        response = decode_timestamp_response(message.payload)
        imprint = response.time_stamp_token.tst_info.message_imprint
        digest = str.join("", (f"{n:02X}" for n in imprint.hashed_message.asNumbers()))
        algorithm = oid_to_hash[imprint.hash_algorithm["algorithm"]]
        key = f"{algorithm}:{digest}"
        queue.put((MessageType.TimeStampResponse, key, response))

    def __enter__(self):
        self.__client.loop_start()


    def __exit__(self, _type, _value, _traceback):
        self.__client.loop_stop()


def process(queue: MpQueue, certificate: str):
    """Process all messages coming from MQTT or the digest FIFO."""

    try:
        with open(certificate, "rb") as f:
            certificate = f.read()

        digest_keys = Queue(maxsize=5)
        timestamp_response_keys = Queue(maxsize=5)

        unverified_digests: dict[str, Digest] = {}
        unused_timestamp_responses: dict[str, TimeStampResp] = {}

        while item := queue.get():
            ty, key, message = item

            match ty:
                case MessageType.Digest:
                    if key in unverified_digests:
                        print(f"DUPLICATE_DIGEST {key}")
                    else:
                        if digest_keys.full():
                            discard_key = digest_keys.get_nowait()
                            if discard_key in unverified_digests:
                                print(f"DISCARD_DIGEST {discard_key}")
                                del unverified_digests[discard_key]

                        digest_keys.put_nowait(key)
                        unverified_digests[key] = message

                case MessageType.TimeStampResponse:
                    if key in unused_timestamp_responses:
                        print(f"DUPLICATE_TIMESTAMP {key}")
                    else:
                        if timestamp_response_keys.full():
                            discard_key = timestamp_response_keys.get_nowait()
                            if discard_key in unused_timestamp_responses:
                                print(f"DISCARD_TIMESTAMP {discard_key}")
                                del unused_timestamp_responses[discard_key]

                        timestamp_response_keys.put_nowait(key)
                        unused_timestamp_responses[key] = message

            for key in set(unverified_digests.keys()).intersection(set(unused_timestamp_responses.keys())):
                digest = unverified_digests.pop(key)
                timestamp_response = unused_timestamp_responses.pop(key)

                try:
                    verify(digest, timestamp_response, certificate)
                    print(f"ACCEPT {key} {digest.first=} {digest.last=}")
                except ValueError as err:
                    print(f"REJECT {key} {err}")

                with open(f"{key}.tsr", "wb") as tsr:
                    tsr.write(der.encoder.encode(timestamp_response))

    except KeyboardInterrupt:
        pass


def main():
    queue = MpQueue()

    fifo_path = "digests-receiver"
    mqtt_broker = "localhost"
    mqtt_port = 1883
    mqtt_topic = "tsr"
    certificate = "tsa.crt"

    with FifoWorker(queue, fifo_path), MqttWorker(queue, mqtt_broker, mqtt_port, mqtt_topic):
        process(queue, certificate)

if __name__ == "__main__":
    main()
