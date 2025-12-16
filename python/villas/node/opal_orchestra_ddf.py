"""
Author: Steffen Vogel <steffen.vogel@opal-rt.com>
SPDX-FileCopyrightText: 2025 OPAL-RT Germany GmbH
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

import argparse
import sys
import xml.etree.ElementTree as ET
from xml.dom import minidom

try:
    import libconf
except ImportError:
    # In case we use a local copy of libconf
    from . import libconf  # type: ignore


def generate_ddf(node_cfg):
    """
    Generate an OPAL-RT Orchestra DDF XML file from a VILLASnode configuration.

    Args:
        node_cfg: Dictionary containing the node configuration
                  from VILLASnode config

    Returns:
        str: XML-encoded string representing the Orchestra DDF
    """
    orchestra = ET.Element("orchestra")

    domain = ET.SubElement(orchestra, "domain", name=node_cfg.domain)

    # Add domain properties
    synchronous = node_cfg.get("synchronous", False)
    ET.SubElement(domain, "synchronous").text = "yes" if synchronous else "no"

    multiple_publish = node_cfg.get("multiple_publish_allowed", False)
    ET.SubElement(domain, "multiplePublishAllowed").text = (
        "yes" if multiple_publish else "no"
    )

    states = node_cfg.get("states", False)
    ET.SubElement(domain, "states").text = "yes" if states else "no"

    # Add connection element
    conn_cfg = node_cfg.get("connection", {})
    add_connection(domain, conn_cfg)

    # Process input signals -> PUBLISH set
    in_cfg = node_cfg.get("in", {})
    in_signals = in_cfg.get("signals", [])

    if in_signals:
        publish_set = ET.SubElement(domain, "set", name="PUBLISH")
        add_signals_to_set(publish_set, in_signals, is_publish=False)

    # Process output signals -> SUBSCRIBE set
    out_cfg = node_cfg.get("out", {})
    out_signals = out_cfg.get("signals", [])

    if out_signals:
        subscribe_set = ET.SubElement(domain, "set", name="SUBSCRIBE")
        add_signals_to_set(subscribe_set, out_signals, is_publish=True)

    # Convert to pretty-printed XML string
    xml_str = ET.tostring(orchestra, encoding="unicode")
    dom = minidom.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent="    ", encoding="UTF-8")

    return pretty_xml.decode("utf-8")


def add_connection(parent_elem, conn_cfg):
    """
    Add a connection element to the parent XML element based
    on connection configuration.

    Args:
        parent_elem: ET.Element to add the connection to
        conn_cfg: Dictionary containing connection configuration
    """
    conn_type = conn_cfg.get("type", "local")

    if conn_type == "local":
        conn_elem = ET.SubElement(
            parent_elem,
            "connection",
            **{
                "type": "local",
                "extcomm": conn_cfg.get("extcomm", "udp"),
                "addrframework": conn_cfg.get("addr_framework", ""),
                "portframework": str(conn_cfg.get("port_framework", 10000)),
                "nicframework": conn_cfg.get("nic_framework", ""),
                "nicclient": conn_cfg.get("nic_client", ""),
                "coreframework": str(conn_cfg.get("core_framework", 0)),
                "coreclient": str(conn_cfg.get("core_client", 0)),
            },
        )

    elif conn_type == "remote":
        conn_elem = ET.SubElement(parent_elem, "connection", type="remote")

        card_elem = ET.SubElement(conn_elem, "card")
        card_elem.text = conn_cfg.get("card", "")

        pciindex_elem = ET.SubElement(conn_elem, "pciindex")
        pciindex_elem.text = str(conn_cfg.get("pci_index", 0))

    elif conn_type == "dolphin":
        conn_elem = ET.SubElement(parent_elem, "connection", type="dolphin")

        nodeidframework_elem = ET.SubElement(conn_elem, "nodeIdFramework")
        nodeidframework_elem.text = str(conn_cfg.get("node_id_framework", 0))

        segmentid_elem = ET.SubElement(conn_elem, "segmentId")
        segmentid_elem.text = str(conn_cfg.get("segment_id", 0))


def add_signals_to_set(parent_elem, signals, is_publish=True):
    """
    Add signals to a PUBLISH or SUBSCRIBE set element.
    Handles nested bus structures based on orchestra_name paths.

    Args:
        parent_elem: ET.Element to add signals to
        signals: List of signal configurations
        is_publish: Boolean indicating if this is for
                    PUBLISH (True) or SUBSCRIBE (False)
    """
    # Group signals by their orchestra names and build nested structure
    signal_tree = {}

    for sig in signals:
        orchestra_name = sig.get("orchestra_name", sig.name)
        orchestra_type = sig.get("orchestra_type", "float64")
        orchestra_index = sig.get("orchestra_index")

        # Split by '/' to handle nested buses
        orchestra_name_parts = orchestra_name.split("/")

        # Build nested structure
        current = signal_tree

        for i, part in enumerate(orchestra_name_parts):
            is_leaf = i == len(orchestra_name_parts) - 1

            if part not in current:
                current[part] = {"_signal": None, "_children": {}}

            if is_leaf:
                signal = current[part]["_signal"]
                if signal is None:
                    if orchestra_index is None:
                        orchestra_index = 0

                    current[part]["_signal"] = {
                        "name": part,
                        "type": orchestra_type,
                        "length": orchestra_index + 1,
                        "default": sig.get("init") if not is_publish else None,
                    }
                else:
                    if orchestra_type != signal["type"]:
                        raise RuntimeError(
                            "Conflicting definitions for signal " + f"'{orchestra_name}'"
                        )

                    index = orchestra_index
                    if index is None:
                        index = signal["length"]

                    if index >= signal["length"]:
                        signal["length"] = index + 1

            else:
                # Navigate to next level
                current = current[part]["_children"]

    # Build XML from tree
    build_xml_from_tree(parent_elem, signal_tree, is_publish)


def build_xml_from_tree(parent_elem, tree, is_publish):
    """
    Recursively build XML elements from the signal tree.

    Args:
        parent_elem: Parent XML element
        tree: Nested dictionary representing signal hierarchy
        is_publish: Boolean for PUBLISH vs SUBSCRIBE
    """
    for name, node in sorted(tree.items()):
        signal = node.get("_signal", None)
        children = node.get("_children", {})

        if signal:
            # Determine if this is a bus (has children) or a simple signal
            is_bus = len(children) > 0

            item = ET.SubElement(parent_elem, "item", name=signal["name"])

            if is_bus:
                ET.SubElement(item, "type").text = "bus"
                # Recursively add children
                build_xml_from_tree(item, children, is_publish)
            else:
                ET.SubElement(item, "type").text = signal["type"]
                length = signal.get("length", 1)
                ET.SubElement(item, "length").text = str(length)

                # Add default for SUBSCRIBE
                if not is_publish:
                    default_val = signal.get("default")
                    if default_val is not None:
                        if signal["type"] == "boolean":
                            default_text = "yes" if default_val else "no"
                        else:
                            default_text = str(default_val)
                    else:
                        if signal["type"] == "boolean":
                            default_text = "no"
                        else:
                            default_text = "0"
                    ET.SubElement(item, "default").text = default_text
        elif children:
            # This is a bus without direct signals
            item = ET.SubElement(parent_elem, "item", name=name)
            ET.SubElement(item, "type").text = "bus"
            build_xml_from_tree(item, children, is_publish)


def parse_args():
    parser = argparse.ArgumentParser(
        description="VILLASnode OPAL Orchestra configuration generator"
    )
    parser.add_argument(
        "--villas-config",
        "-i",
        type=str,
        help="Path to VILLASnode configuration file",
    )
    parser.add_argument(
        "--orchestra-ddf",
        "-o",
        type=str,
        help="Path to output OPAL-RT Orchestra DDF file",
    )
    parser.add_argument(
        "node",
        type=str,
        help="Name of the node to generate OPAL-RT Orchestra DDF for",
    )

    return parser.parse_args()


def main():
    args = parse_args()

    if args.villas_config is None:
        villas_cfg = libconf.load(sys.stdin)
    else:
        with open(args.villas_config) as f:
            villas_cfg = libconf.load(f)

    node_cfg = villas_cfg.nodes.get(args.node)
    if node_cfg is None:
        raise RuntimeError(f"Node '{args.node}' not found in configuration")

    ddf = generate_ddf(node_cfg)

    if args.orchestra_ddf is None:
        sys.stdout.write(ddf)
    else:
        with open(args.orchestra_ddf, "w") as f:
            f.write(ddf)


if __name__ == "__main__":
    main()
