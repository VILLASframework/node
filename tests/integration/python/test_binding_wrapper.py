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


class BindingWrapperIntegrationTests(unittest.TestCase):
    def setUp(self):
        try:
            self.config = json.dumps(test_node_config, indent=2)
            self.node_uuid = str(uuid.uuid4())
            self.test_node = b.node_new(self.config, self.node_uuid)
        except Exception as e:
            self.fail(f"new_node err: {e}")

    def tearDown(self):
        try:
            b.node_stop(self.test_node)
            b.node_destroy(self.test_node)
        except Exception as e:
            self.fail(f"node cleanup error: {e}")

    def test_activity_changes(self):
        try:
            b.node_check(self.test_node)
            b.node_prepare(self.test_node)
            # starting twice
            self.assertEqual(0, b.node_start(self.test_node))

            # check if the node is running
            self.assertTrue(b.node_is_enabled(self.test_node))

            # pausing twice
            self.assertEqual(0, b.node_pause(self.test_node))
            self.assertEqual(-1, b.node_pause(self.test_node))

            # resuming
            self.assertEqual(0, b.node_resume(self.test_node))

            # stopping twice
            self.assertEqual(0, b.node_stop(self.test_node))
            self.assertEqual(0, b.node_stop(self.test_node))

            # restarting
            b.node_restart(self.test_node)

            # check if everything still works after restarting
            b.node_pause(self.test_node)
            b.node_resume(self.test_node)
            b.node_stop(self.test_node)
            b.node_start(self.test_node)
        except Exception as e:
            self.fail(f" err: {e}")

    def test_reverse_node(self):
        try:
            self.assertEqual(1, b.node_input_signals_max_cnt(self.test_node))
            self.assertEqual(0, b.node_output_signals_max_cnt(self.test_node))

            self.assertEqual(0, b.node_reverse(self.test_node))

            # input and output hooks/details are not reversed
            # input and output are reversed, can be seen with wireshark and
            #   function test_rw_socket_and_reverse() below
            self.assertEqual(1, b.node_input_signals_max_cnt(self.test_node))
            self.assertEqual(0, b.node_output_signals_max_cnt(self.test_node))
        except Exception as e:
            self.fail(f"Reversing node in and output failed: {e}")

    # Test if a node can be recreated with the string from node_to_json_str
    # node_to_json_str has a wrong config format causing the config string
    # to create a node without a name
    # uuid can not match
    def test_config_from_string(self):
        try:
            config_str = b.node_to_json_str(self.test_node)
            config_obj = json.loads(config_str)

            config_copy_str = json.dumps(config_obj, indent=2)

            test_node = b.node_new(config_copy_str)

            self.assertEqual(
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ",
                    "",
                    b.node_name_full(test_node),
                ),
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ",
                    "",
                    b.node_name_full(self.test_node),
                ),
            )
        except Exception as e:
            self.fail(f" err: {e}")

    def test_rw_socket_and_reverse(self):
        try:
            data_str = json.dumps(send_recv_test, indent=2)
            data = json.loads(data_str)

            test_nodes = {}
            for name, content in data.items():
                obj = {name: content}
                config = json.dumps(obj, indent=2)
                id = str(uuid.uuid4())

                test_nodes[name] = b.node_new(config, id)

            for node in test_nodes.values():
                if b.node_check(node):
                    raise RuntimeError("Failed to verify node configuration")
                if b.node_prepare(node):
                    raise RuntimeError(
                        f"Failed to verify {b.node_name(node)} node config"
                    )
                b.node_start(node)

            # Arrays to store samples
            send_smpls = b.SamplesArray(1)
            intmdt_smpls = b.SamplesArray(100)
            recv_smpls = b.SamplesArray(100)

            for i in range(100):
                # Generate signals and send over send_socket
                self.assertEqual(
                    b.node_read(test_nodes["signal_generator"], send_smpls, 2, 1),
                    1,
                )
                self.assertEqual(
                    b.node_write(test_nodes["send_socket"], send_smpls, 1), 1
                )

            # read received signals and send them to recv_socket
            self.assertEqual(
                b.node_read(test_nodes["intmdt_socket"], intmdt_smpls, 2, 100),
                100,
            )
            self.assertEqual(
                b.node_write(test_nodes["intmdt_socket"], intmdt_smpls[0:50], 50),
                50,
            )
            self.assertEqual(
                b.node_write(test_nodes["intmdt_socket"], intmdt_smpls[50:100], 50),
                50,
            )

            # confirm rev_socket signals
            self.assertEqual(
                b.node_read(test_nodes["recv_socket"], recv_smpls[0:50], 2, 50),
                50,
            )
            self.assertEqual(
                b.node_read(test_nodes["recv_socket"], recv_smpls[50:100], 2, 50),
                50,
            )

            # reversing in and outputs
            # stopping the socket is necessary to clean up buffers
            # starting the node again will bind the reversed socket addresses
            #   this can be confirmed when observing network traffic
            #   node details do not represent this properly as of now
            for node in test_nodes.values():
                b.node_reverse(node)
                b.node_stop(node)

            for node in test_nodes.values():
                b.node_start(node)

            # if another 50 samples have not been allocated,
            # sending 100 at once is impossible with recv_smpls
            self.assertEqual(
                b.node_write(test_nodes["recv_socket"], recv_smpls, 100), 100
            )
            # try writing as full slice
            self.assertEqual(
                b.node_write(test_nodes["intmdt_socket"], recv_smpls[0:100], 100),
                100,
            )

            # cleanup
            for node in test_nodes.values():
                b.node_stop(node)
                b.node_destroy(node)

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
