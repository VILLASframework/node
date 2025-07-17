"""
Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import json
import re
import unittest
import uuid
import villas_node as vn


class SimpleWrapperTests(unittest.TestCase):
    def setUp(self):
        try:
            self.node_uuid = str(uuid.uuid4())
            self.config = json.dumps(test_node_config, indent=2)
            self.test_node = vn.node_new(self.node_uuid, self.config)
        except Exception as e:
            self.fail(f"new_node err: {e}")

    def tearDown(self):
        try:
            vn.node_stop(self.test_node)
            vn.node_destroy(self.test_node)
        except Exception as e:
            self.fail(f"node cleanup error: {e}")

    def test_start(self):
        try:
            self.assertEqual(0, vn.node_start(self.test_node))
            # socket node will cause a RuntimeError
            # the behavior is not consistent for each node
            # with self.assertRaises((AssertionError, RuntimeError)):
            #     vn.node_start(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_new(self):
        try:
            node_uuid = str(uuid.uuid4())
            node_config = json.dumps(test_node_config, indent=2)
            node = vn.node_new(node_uuid, node_config)
            self.assertIsNotNone(node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_check(self):
        try:
            vn.node_check(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_prepare(self):
        try:
            vn.node_prepare(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_is_enabled(self):
        try:
            self.assertTrue(vn.node_is_enabled(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_pause(self):
        try:
            self.assertEqual(-1, vn.node_pause(self.test_node))
            self.assertEqual(-1, vn.node_pause(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_resume(self):
        try:
            self.assertEqual(0, vn.node_resume(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_stop(self):
        try:
            self.assertEqual(0, vn.node_stop(self.test_node))
            self.assertEqual(0, vn.node_stop(self.test_node))
            vn.node_restart(self.test_node)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_restart(self):
        try:
            self.assertEqual(0, vn.node_restart(self.test_node))
            self.assertEqual(0, vn.node_restart(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name(self):
        try:
            # remove color codes before checking for equality
            self.assertEqual(
                "test_node(socket)",
                re.sub(r"\x1b\[[0-9;]*m", "", vn.node_name(self.test_node)),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name_short(self):
        try:
            print()
            print(f"node name short: {vn.node_name_short(self.test_node)}")
            print()
            self.assertEqual("test_node", vn.node_name_short(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_name_full(self):
        try:
            self.assertEqual(
                "test_node(socket)"
                + ": uuid="
                + self.node_uuid
                + ", #in.signals=1/1, #in.hooks=0, #out.hooks=0, in.vectorize=1, out.vectorize=1, out.netem=no, layer=udp, in.address=0.0.0.0:12000, out.address=127.0.0.1:12001",
                re.sub(r"\x1b\[[0-9;]*m", "", vn.node_name_full(self.test_node)),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_details(self):
        try:
            self.assertEqual(
                "layer=udp, in.address=0.0.0.0:12000, out.address=127.0.0.1:12001",
                vn.node_details(self.test_node),
            )
        except Exception as e:
            self.fail(f"err: {e}")

    def test_input_signals_max_cnt(self):
        try:
            self.assertEqual(1, vn.node_input_signals_max_cnt(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_output_signals_max_cnt(self):
        try:
            self.assertEqual(0, vn.node_output_signals_max_cnt(self.test_node))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_node_is_valid_name(self):
        try:
            self.assertFalse(vn.node_is_valid_name(""))
            self.assertFalse(vn.node_is_valid_name("###"))
            self.assertFalse(vn.node_is_valid_name("v@l:d T3xt w;th invalid symb#ls"))
            self.assertFalse(vn.node_is_valid_name("33_characters_long_string_invalid"))
            self.assertTrue(vn.node_is_valid_name("32_characters_long_strings_valid"))
            self.assertTrue(vn.node_is_valid_name("valid_name"))
        except Exception as e:
            self.fail(f"err: {e}")

    def test_reverse(self):
        try:
            self.assertEqual(0, vn.node_reverse(self.test_node))
            self.assertEqual(0, vn.node_reverse(self.test_node))
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

if __name__ == "__main__":
    unittest.main()
