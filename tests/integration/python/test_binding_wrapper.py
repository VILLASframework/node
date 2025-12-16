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


class BindingWrapperIntegrationTests(unittest.TestCase):
    def setUp(self):
        try:
            self.config = json.dumps(test_node_config, indent=2)
            self.node_uuid = str(uuid.uuid4())
            self.test_node = Node(self.config, self.node_uuid)
        except Exception as e:
            self.fail(f"new_node err: {e}")

    def test_activity_changes(self):
        try:
            self.test_node.check()
            self.test_node.prepare()
            # starting twice
            self.assertEqual(0, self.test_node.start())

            # check if the node is running
            self.assertTrue(self.test_node.is_enabled())

            # pausing twice
            self.assertEqual(0, self.test_node.pause())
            self.assertEqual(-1, self.test_node.pause())

            # resuming
            self.assertEqual(0, self.test_node.resume())

            # stopping twice
            self.assertEqual(0, self.test_node.stop())
            self.assertEqual(0, self.test_node.stop())

            # restarting
            self.test_node.restart()

            # check if everything still works after restarting
            self.test_node.pause()
            self.test_node.resume()
            self.test_node.stop()
            self.test_node.start()
        except Exception as e:
            self.fail(f" err: {e}")

    def test_reverse_node(self):
        try:
            self.assertEqual(1, self.test_node.input_signals_max_cnt())
            self.assertEqual(0, self.test_node.output_signals_max_cnt())

            self.assertEqual(0, self.test_node.reverse())

            # input and output hooks/details are not reversed
            # input and output are reversed, can be seen with wireshark and
            #   function test_rw_socket_and_reverse() below
            self.assertEqual(1, self.test_node.input_signals_max_cnt())
            self.assertEqual(0, self.test_node.output_signals_max_cnt())
        except Exception as e:
            self.fail(f"Reversing node in and output failed: {e}")

    # Test if a node can be recreated with the string from node_to_json_str
    # node_to_json_str has a wrong config format causing the config string
    # to create a node without a name
    # uuid can not match
    def test_config_from_string(self):
        try:
            config_str = self.test_node.to_json_str()
            config_obj = json.loads(config_str)

            config_copy_str = json.dumps(config_obj, indent=2)

            test_node = Node(config_copy_str)

            self.assertEqual(
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ",
                    "",
                    test_node.name_full(),
                ),
                re.sub(
                    r"^[^:]+: uuid=[0-9a-fA-F-]+, ",
                    "",
                    self.test_node.name_full(),
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

                test_nodes[name] = Node(config, id)

            for node in test_nodes.values():
                if node.check():
                    raise RuntimeError("Failed to verify node configuration")
                if node.prepare():
                    raise RuntimeError(f"Failed to verify {node.name()} node config")
                node.start()

            for i in range(100):
                # Generate signals and send over send_socket
                self.assertEqual(test_nodes["signal_generator"][i].read_from(2, 1), 1)
                self.assertEqual(
                    test_nodes["send_socket"][i].write_to(
                        test_nodes["signal_generator"], 1
                    ),
                    1,
                )
            self.assertEqual(test_nodes["signal_generator"].sample_length(0), 2)

            # read received signals and send them to recv_socket
            self.assertEqual(test_nodes["intmdt_socket"].read_from(2, 100), 100)
            self.assertEqual(
                test_nodes["intmdt_socket"][:30].write_to(
                    test_nodes["intmdt_socket"], 30
                ),
                30,
            )
            self.assertEqual(
                test_nodes["intmdt_socket"][30:].write_to(test_nodes["intmdt_socket"]),
                70,
            )
            # print(len(test_nodes["intmdt_socket"]._smps))

            # confirm rev_socket signals
            self.assertEqual(test_nodes["recv_socket"].read_from(2, 30), 30)
            self.assertEqual(test_nodes["recv_socket"][30].read_from(2, 70), 70)

            # reversing in and outputs
            # stopping the socket is necessary to clean up buffers
            # starting the node again will bind the reversed socket addresses
            #   this can be confirmed when observing network traffic
            #   node details do not represent this properly as of now
            for node in test_nodes.values():
                node.reverse()
                node.stop()

            for node in test_nodes.values():
                node.start()

            # if another 30+70 samples are not allocated,
            # sending 100 at once is impossible
            self.assertEqual(
                test_nodes["recv_socket"].write_to(test_nodes["recv_socket"], 100),
                100,
            )
            # try writing as full slice
            self.assertEqual(
                test_nodes["intmdt_socket"][0:100].write_to(
                    test_nodes["recv_socket"], 100
                ),
                100,
            )

        except Exception as e:
            self.fail(f" err: {e}")

    def test_sample_pack_unpack(self):
        try:
            self.test_node.pack_from(0, [0.01, 1.01, 2.01, 3.01, 4.01])
            self.test_node[1].pack_from(
                [1.01, 2.01, 3.01, 4.01, 5.01], int(1e9), int(1e9) + 100
            )
            self.test_node[2].pack_from(42, int(1e9), int(1e9) + 100)
            self.test_node[3].pack_from(self.test_node[1], int(1e9), int(1e9) + 100)
            self.test_node[2].unpack_to(self.test_node[1], int(1e9), int(1e9) + 100)
            self.assertEqual([42.0], self.test_node[1].details()["data"])
            self.test_node[0].unpack_to(self.test_node[1], int(2e9), int(2e9) + 100)
            self.assertEqual(
                [0.01, 1.01, 2.01, 3.01, 4.01],
                self.test_node[1].details()["data"],
            )
            self.test_node[0].unpack_to(self.test_node[2], int(2e9), int(2e9) + 100)
            self.test_node[0].unpack_to(self.test_node[4], int(2e9), int(2e9) + 100)
            self.test_node[1].unpack_to(self.test_node[2], int(2e9), int(2e9) + 100)
        except Exception as e:
            self.fail(f"err: {e}")

    def test_samplesarray_size(self):
        try:
            node_config = json.dumps(test_node_config, indent=2)
            node_uuid = str(uuid.uuid4())
            node = Node(node_config, node_uuid, 100)
            self.assertEqual(len(node), 100)
            node[199].pack_from([1.01, 2.01, 3.01, 4.01, 5.01], int(1e9), int(1e9) + 100)
            self.assertEqual(len(node), 200)
            node[199].unpack_to(node[299], int(2e9), int(2e9) + 100)
            self.assertEqual(len(node), 300)
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
