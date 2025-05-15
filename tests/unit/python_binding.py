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

    def test_activity_changes(self):
        try:
            vn.node_check(self.test_node)
            vn.node_prepare(self.test_node)
            # starting twice
            self.assertEqual(0, vn.node_start(self.test_node))
            # with self.assertRaises((AssertionError, RuntimeError)):
            #     vn.node_start(self.test_node)

            # check if the node is running
            self.assertTrue(vn.node_is_enabled(self.test_node))

            # pausing twice
            self.assertEqual(0, vn.node_pause(self.test_node))
            self.assertEqual(-1, vn.node_pause(self.test_node))

            # resuming
            self.assertEqual(0, vn.node_resume(self.test_node))

            # stopping twice
            self.assertEqual(0, vn.node_stop(self.test_node))
            # self.assertEqual(-1, vn.node_stop(self.test_node))

            # # restarting
            # vn.node_restart(self.test_node)

            # check if everything still works after restarting
            # vn.node_pause(self.test_node)
            # vn.node_resume(self.test_node)
            # vn.node_stop(self.test_node)

            # terminating the node
            vn.node_destroy(self.test_node)
        except Exception as e:
            self.fail(f" err: {e}")

    def test_details(self):
        try:
            # remove color codes before checking for equality
            self.assertEqual(
                "test_node(socket)",
                re.sub(r"\x1b\[[0-9;]*m", "", vn.node_name(self.test_node)),
            )

            # node_name_short is bugged
            # self.assertEqual('', vn.node_name_short(self.test_node))
            self.assertEqual(
                vn.node_name(self.test_node)
                + ": uuid="
                + self.node_uuid
                + ", #in.signals=1/1, #in.hooks=0, #out.hooks=0, in.vectorize=1, out.vectorize=1, out.netem=no, layer=udp, in.address=0.0.0.0:12000, out.address=127.0.0.1:12001",
                vn.node_name_full(self.test_node),
            )

            self.assertEqual(
                "layer=udp, in.address=0.0.0.0:12000, out.address=127.0.0.1:12001",
                vn.node_details(self.test_node),
            )

            self.assertEqual(1, vn.node_input_signals_max_cnt(self.test_node))
            self.assertEqual(0, vn.node_output_signals_max_cnt(self.test_node))
        except Exception as e:
            self.fail(f" err: {e}")

    # Test whether or not the json object is created by the wrapper and the <json> module
    def test_json_obj(self):
        try:
            node_config = vn.node_to_json(self.test_node)
            node_config_str = json.dumps(node_config)
            node_config_parsed = json.loads(node_config_str)

            self.assertEqual(node_config, node_config_parsed)
        except Exception as e:
            self.fail(f" err: {e}")

    # Test whether or not a node can be recreated with the string from node_to_json_str
    # node_to_json_str has a wrong config format, thus the name config string will create, as of now,
    # a node without a name
    # uuid can not match
    def test_config_from_string(self):
        try:
            config_str = vn.node_to_json_str(self.test_node)
            config_obj = json.loads(config_str)

            config_copy_str = json.dumps(config_obj, indent=2)

            test_node = vn.node_new("", config_copy_str)

            self.assertEqual(
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ", "", vn.node_name_full(test_node)
                ),
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ",
                    "",
                    vn.node_name_full(self.test_node),
                ),
            )
        except Exception as e:
            self.fail(f" err: {e}")

    def test_rw_socket(self):
        try:
            data_str = json.dumps(send_recv_test, indent=2)
            data = json.loads(data_str)

            test_nodes = {}
            for name, content in data.items():
                obj = {name: content}
                config = json.dumps(obj, indent=2)
                id = str(uuid.uuid4())

                test_nodes[name] = vn.node_new(id, config)

            for node in test_nodes.values():
                if vn.node_check(node):
                    raise RuntimeError("Failed to verify node configuration")
                if vn.node_prepare(node):
                    raise RuntimeError(
                        f"Failed to verify {vn.node_name(node)} node configuration"
                    )
                vn.node_start(node)

            # Arrays to store samples
            send_smpls = vn.smps_array(1)
            intmdt_smpls = vn.smps_array(100)
            recv_smpls = vn.smps_array(100)

            for i in range(100):
                send_smpls[0] = vn.sample_alloc(2)
                intmdt_smpls[i] = vn.sample_alloc(2)
                recv_smpls[i] = vn.sample_alloc(2)

                # Generate signals and send over send_socket
                self.assertEqual(
                    vn.node_read(test_nodes["signal_generator"], send_smpls, 1), 1
                )
                self.assertEqual(
                    vn.node_write(test_nodes["send_socket"], send_smpls, 1), 1
                )

            # read received signals and send them to recv_socket
            self.assertEqual(
                vn.node_read(test_nodes["intmdt_socket"], intmdt_smpls, 100), 100
            )
            self.assertEqual(
                vn.node_write(test_nodes["intmdt_socket"], intmdt_smpls, 100), 100
            )

            # confirm rev_socket signals
            self.assertEqual(
                vn.node_read(test_nodes["recv_socket"], recv_smpls, 100), 100
            )

        except Exception as e:
            self.fail(f" err: {e}")


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

send_recv_test = {
    "send_socket": {
        "type": "socket",
        "format": "protobuf",
        "layer": "udp",
        "in": {
            "address": "127.0.0.1:65532",
            "signals": [
                {"name": "voltage", "type": "float", "unit": "V"},
                {"name": "current", "type": "float", "unit": "A"},
            ],
        },
        "out": {
            "address": "127.0.0.1:65533",
            "netem": {"enabled": False},
            "multicast": {"enabled": False},
        },
    },
    "intmdt_socket": {
        "type": "socket",
        "format": "protobuf",
        "layer": "udp",
        "in": {
            "address": "127.0.0.1:65533",
            "signals": [
                {"name": "voltage", "type": "float", "unit": "V"},
                {"name": "current", "type": "float", "unit": "A"},
            ],
        },
        "out": {
            "address": "127.0.0.1:65534",
            "netem": {"enabled": False},
            "multicast": {"enabled": False},
        },
    },
    "recv_socket": {
        "type": "socket",
        "format": "protobuf",
        "layer": "udp",
        "in": {
            "address": "127.0.0.1:65534",
            "signals": [
                {"name": "voltage", "type": "float", "unit": "V"},
                {"name": "current", "type": "float", "unit": "A"},
            ],
        },
        "out": {
            "address": "127.0.0.1:65535",
            "netem": {"enabled": False},
            "multicast": {"enabled": False},
        },
    },
    "signal_generator": {
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
    },
}

if __name__ == "__main__":
    unittest.main()
