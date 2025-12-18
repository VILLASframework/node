"""
Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import json
import re
import unittest
import uuid
from villas.node.binding import Node


class BindingWrapperUnitTests(unittest.TestCase):
    def setUp(self):
        try:
            self.config = json.dumps(test_node_config, indent=2)
            self.node_uuid = str(uuid.uuid4())
            self.test_node = Node(self.config, self.node_uuid)
            config = json.dumps(signal_test_node_config, indent=2)
            node_uuid = str(uuid.uuid4())
            self.signal_test_node = Node(config, node_uuid)
        except Exception as e:
            self.fail(f"new_node err: {e}")

    def test_start(self):
        try:
            self.assertEqual(0, self.test_node.start())
            self.assertEqual(0, self.signal_test_node.start())
        except Exception as e:
            self.fail(f"err: {e}")

    @unittest.skip(
        """Starting a socket twice will result in a RuntimeError.
    Thise will leave the socket IP bound and may mess with other tests.
    The behavior is Node specific."""
    )
    def test_start_err(self):
        try:
            self.assertEqual(0, self.test_node.start())
            with self.assertRaises((AssertionError, RuntimeError)):
                self.test_node.start()
        except Exception as e:
            self.fail(f"err: {e}")

    def test_new(self):
        try:
            node_config = json.dumps(test_node_config, indent=2)
            node_uuid = str(uuid.uuid4())
            node = Node(node_config, node_uuid)
            self.assertIsNotNone(node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_check(self):
        try:
            self.test_node.check()
        except Exception as e:
            self.fail(f"err: {e}")

    def test_prepare(self):
        try:
            self.test_node.prepare()
        except Exception as e:
            self.fail(f"err: {e}")

    def test_is_enabled(self):
        try:
            self.assertTrue(self.test_node.is_enabled())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_pause(self):
        try:
            self.assertEqual(-1, self.test_node.pause())
            self.assertEqual(-1, self.test_node.pause())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_resume(self):
        try:
            self.assertEqual(0, self.test_node.resume())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_stop(self):
        try:
            self.assertEqual(0, self.test_node.start())
            self.assertEqual(0, self.test_node.stop())
            self.assertEqual(0, self.test_node.stop())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_restart(self):
        try:
            self.assertEqual(0, self.test_node.restart())
            self.assertEqual(0, self.test_node.restart())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name(self):
        try:
            # remove color codes before checking for equality
            self.assertEqual(
                "test_node(socket)",
                re.sub(r"\x1b\[[0-9;]*m", "", self.test_node.name()),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name_short(self):
        try:
            self.assertEqual("test_node", self.test_node.name_short())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name_full(self):
        try:
            node = self.test_node
            self.assertEqual(
                "test_node(socket)"
                + ": uuid="
                + self.node_uuid
                + ", #in.signals=1/1, #in.hooks=0, #out.hooks=0"
                + ", in.vectorize=1, out.vectorize=1, out.netem=no, layer=udp"
                + ", in.address=0.0.0.0:12000, out.address=127.0.0.1:12001",
                re.sub(r"\x1b\[[0-9;]*m", "", node.name_full()),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_details(self):
        try:
            self.assertEqual(
                "layer=udp, "
                + "in.address=0.0.0.0:12000, "
                + "out.address=127.0.0.1:12001",
                self.test_node.details(),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_to_json(self):
        try:
            if not isinstance(self.test_node.to_json(), dict):
                self.fail("Not a JSON object (dict)")
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_to_json_str(self):
        try:
            json.loads(self.test_node.to_json_str())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_input_signals_max_cnt(self):
        try:
            self.assertEqual(1, self.test_node.input_signals_max_cnt())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_output_signals_max_cnt(self):
        try:
            self.assertEqual(0, self.test_node.output_signals_max_cnt())
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_is_valid_name(self):
        try:
            invalid_names = [
                "",
                "###",
                "v@l:d T3xt w;th invalid symb#ls",
                "33_characters_long_string_invalid",
            ]
            valid_names = ["32_characters_long_strings_valid", "valid_name"]

            for name in invalid_names:
                self.assertFalse(Node.is_valid_name(name))
            for name in valid_names:
                self.assertTrue(Node.is_valid_name(name))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_reverse(self):
        try:
            # socket has reverse() implemented, expected return 0
            self.assertEqual(0, self.test_node.reverse())
            self.assertEqual(0, self.test_node.reverse())

            # signal.v2 has not reverse() implemented, expected return 1
            self.assertEqual(-1, self.signal_test_node.reverse())
            self.assertEqual(-1, self.signal_test_node.reverse())
        except Exception as e:
            self.fail(f"err: {e}")


test_node_config = {
    "test_node": {
        "type": "socket",
        "format": "villas.binary",
        "layer": "udp",
        "in": {
            "address": "*:12000",
            "signals": [{"name": "tap_position", "type": "integer", "init": 0}],
        },
        "out": {"address": "127.0.0.1:12001"},
    }
}

signal_test_node_config = {
    "signal_test_node": {
        "type": "signal.v2",
        "limit": 100,
        "rate": 10,
        "in": {
            "signals": [
                {
                    "amplitude": 2,
                    "name": "voltage",
                    "phase": 90,
                    "signal": "sine",
                    "type": "float",
                    "unit": "V",
                },
                {
                    "amplitude": 1,
                    "name": "current",
                    "phase": 0,
                    "signal": "sine",
                    "type": "float",
                    "unit": "A",
                },
            ],
            "hooks": [{"type": "print", "format": "villas.human"}],
        },
    }
}

if __name__ == "__main__":
    unittest.main()
