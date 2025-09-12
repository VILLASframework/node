"""
Author: Steffen Vogel <post@steffenvogel.de>
SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import datetime
import json
import logging
import os
import signal
import subprocess
from tempfile import NamedTemporaryFile

import requests

LOGGER = logging.getLogger("villas.node")


class Node(object):
    api_version = "v2"

    def __init__(
        self,
        api_url=None,
        log_filename=None,
        config_filename=None,
        config={},
        executable="villas-node",
        **kwargs,
    ):
        self.api_url = api_url
        self.log_filename = log_filename
        self.executable = executable

        if config_filename and config:
            raise RuntimeError(
                "Can't provide config_filename and " "config at the same time!"
            )

        if config_filename:
            with open(config_filename) as f:
                self.config = json.load(f)
        else:
            self.config = config

        # Try to deduct api_url from config
        if self.api_url is None:
            port = config.get("http", {}).get("port")
            if port is None:
                port = 80 if os.getuid() == 0 else 8080

            self.api_url = f"http://localhost:{port}"

    def start(self):
        self.config_file = NamedTemporaryFile(mode="w+", suffix=".json")

        json.dump(self.config, self.config_file)

        self.config_file.flush()

        if self.log_filename is None:
            now = datetime.datetime.now()
            fmt = "villas-node_%Y-%m-%d_%H-%M-%S.log"
            self.log_filename = now.strftime(fmt)

        self.log = open(self.log_filename, "w+")

        LOGGER.info(
            f"Starting VILLASnode instance with config: {self.config_file.name}"  # noqa: E501
        )

        self.child = subprocess.Popen(
            [self.executable, self.config_file.name],
            stdout=self.log,
            stderr=self.log,
        )

    def pause(self):
        LOGGER.info("Pausing VILLASnode instance")
        self.child.send_signal(signal.SIGSTOP)

    def resume(self):
        LOGGER.info("Resuming VILLASnode instance")
        self.child.send_signal(signal.SIGCONT)

    def stop(self):
        LOGGER.info("Stopping VILLASnode instance")
        self.child.send_signal(signal.SIGTERM)
        self.child.wait()

        self.log.close()

    def restart(self):
        LOGGER.info("Restarting VILLASnode instance")
        self.request("restart", method="POST")

    @property
    def active_config(self):
        return self.request("config")

    @property
    def nodes(self):
        return self.request("nodes")

    @property
    def paths(self):
        return self.request("paths")

    @property
    def status(self):
        return self.request("status")

    def load_config(self, i):
        if type(i) is dict:
            cfg = i
        elif type(i) is str:
            cfg = json.loads(i)
        elif hasattr(i, "read"):  # file-like?
            cfg = json.load(i)
        else:
            raise TypeError()

        req = {"config": cfg}

        self.request("restart", method="POST", json=req)

    def request(self, action, method="GET", **args):
        if "timeout" not in args:
            args["timeout"] = 1

        r = requests.request(
            method, f"{self.api_url}/api/{self.api_version}/{action}", **args
        )
        r.raise_for_status()

        return r.json()

    def get_local_version(self):
        ver = subprocess.check_output([self.executable, "-V"])

        return ver.decode("ascii").rstrip()

    def get_version(self):
        resp = self.request("status")

        return resp["version"]

    def is_running(self):
        if self.child is None:
            return False
        else:
            return self.child.poll() is None
