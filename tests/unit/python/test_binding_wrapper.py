"""
Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import json
import re
import unittest
import uuid
import villas.node.binding as b


class BindingWrapperUnitTests(unittest.TestCase):
    def setUp(self):
        try:
            self.config = json.dumps(test_node_config, indent=2)
            self.node_uuid = str(uuid.uuid4())
            self.test_node = b.node_new(self.config, self.node_uuid)
            config = json.dumps(signal_test_node_config, indent=2)
            node_uuid = str(uuid.uuid4())
            self.signal_test_node = b.node_new(config, node_uuid)
        except Exception as e:
            self.fail(f"new_node err: {e}")

    def tearDown(self):
        try:
            b.node_stop(self.test_node)
            b.node_destroy(self.test_node)
            b.node_stop(self.signal_test_node)
            b.node_destroy(self.signal_test_node)
        except Exception as e:
            self.fail(f"node cleanup error: {e}")

    def test_start(self):
        try:
            self.assertEqual(0, b.node_start(self.test_node))
            self.assertEqual(0, b.node_start(self.signal_test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    @unittest.skip(
        """Starting a socket twice will result in a RuntimeError.
    Thise will leave the socket IP bound and may mess with other tests.
    The behavior is Node specific."""
    )
    def test_start_err(self):
        try:
            self.assertEqual(0, b.node_start(self.test_node))
            with self.assertRaises((AssertionError, RuntimeError)):
                b.node_start(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_new(self):
        try:
            node_config = json.dumps(test_node_config, indent=2)
            node_uuid = str(uuid.uuid4())
            node = b.node_new(node_config, node_uuid)
            self.assertIsNotNone(node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_check(self):
        try:
            b.node_check(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_prepare(self):
        try:
            b.node_prepare(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_is_enabled(self):
        try:
            self.assertTrue(b.node_is_enabled(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_pause(self):
        try:
            self.assertEqual(-1, b.node_pause(self.test_node))
            self.assertEqual(-1, b.node_pause(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_resume(self):
        try:
            self.assertEqual(0, b.node_resume(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_stop(self):
        try:
            self.assertEqual(0, b.node_start(self.test_node))
            self.assertEqual(0, b.node_stop(self.test_node))
            self.assertEqual(0, b.node_stop(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_restart(self):
        try:
            self.assertEqual(0, b.node_restart(self.test_node))
            self.assertEqual(0, b.node_restart(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name(self):
        try:
            # remove color codes before checking for equality
            self.assertEqual(
                "test_node(socket)",
                re.sub(r"\x1b\[[0-9;]*m", "", b.node_name(self.test_node)),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name_short(self):
        try:
            self.assertEqual("test_node", b.node_name_short(self.test_node))
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
                re.sub(r"\x1b\[[0-9;]*m", "", b.node_name_full(node)),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_details(self):
        try:
            self.assertEqual(
                "layer=udp, "
                + "in.address=0.0.0.0:12000, "
                + "out.address=127.0.0.1:12001",
                b.node_details(self.test_node),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_to_json(self):
        try:
            if not isinstance(b.node_to_json(self.test_node), dict):
                self.fail("Not a JSON object (dict)")
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_to_json_str(self):
        try:
            json.loads(b.node_to_json_str(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_input_signals_max_cnt(self):
        try:
            self.assertEqual(1, b.node_input_signals_max_cnt(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_output_signals_max_cnt(self):
        try:
            self.assertEqual(0, b.node_output_signals_max_cnt(self.test_node))
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
                self.assertFalse(b.node_is_valid_name(name))
            for name in valid_names:
                self.assertTrue(b.node_is_valid_name(name))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_reverse(self):
        try:
            # socket has reverse() implemented, expected return 0
            self.assertEqual(0, b.node_reverse(self.test_node))
            self.assertEqual(0, b.node_reverse(self.test_node))

            # signal.v2 has not reverse() implemented, expected return 1
            self.assertEqual(-1, b.node_reverse(self.signal_test_node))
            self.assertEqual(-1, b.node_reverse(self.signal_test_node))
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
