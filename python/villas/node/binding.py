"""
Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from typing import Any, Optional, Union

import functools
import json
import logging
import weakref

import villas.node.python_binding as vn

Capsule = Any
logger = logging.getLogger("villas.node")

# helper functions


# function decorator for optional node_compat function calls
#   that would return -1 if a function is not implemented
def _warn_if_not_implemented(func):
    """
    Decorator to warn if specific `node_*()` functions are not implemented.

    Returns:
        Wrapping function that logs a warning if the return value is -1.
    """

    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        ret = func(self, *args, **kwargs)
        if ret == -1 and hasattr(self, "_hndle") and self._hndle is not None:
            msg = (
                f"[\033[93mWarning\033[0m]: Function '{func.__name__}()' "
                + "is not implemented for node type "
                + f"'{vn.node_name(self._hndle)}'."
            )
            logger.warning(msg)
        return ret

    return wrapper


# node API bindings


class Node:
    class _SampleSlice:
        """
        Helper class to achieve overloaded functions.
        Nodes accessed via the `[]` operator, it always returns a _SampleSlice.
        """

        def __init__(self, node, idx):
            self.node = weakref.proxy(node)
            self.idx = idx

        def details(self):
            return self.node.sample_details(self.idx)

        def read_from(self, sample_length, count=None):
            return self.node.read_from(sample_length, count, self.idx)

        def write_to(self, node, count=None):
            return self.node.write_to(node, count, self.idx)

        def pack_from(
            self,
            values: Union[float, list[float], Capsule],
            ts_origin: Optional[int] = None,
            ts_received: Optional[int] = None,
            seq: int = 0,
        ):
            if isinstance(values, self.__class__):
                return self.node.pack_from(
                    self.idx,
                    values.node._smps[values.idx],
                    ts_origin,
                    ts_received,
                    seq,
                )
            else:
                return self.node.pack_from(
                    self.idx, values, ts_origin, ts_received, seq
                )

        def unpack_to(
            self,
            target: Capsule,
            ts_origin: Optional[int] = None,
            ts_received: Optional[int] = None,
        ):
            if isinstance(target, self.__class__):
                return self.node.unpack_to(
                    self.idx,
                    target.node,
                    target.idx,
                    ts_origin,
                    ts_received,
                )
            else:
                raise ValueError(
                    "The destination must be an existing Node with an index!"
                )

    # helper functions
    @staticmethod
    def _ensure_capacity(smps, cap: int):
        """
        Resize SamplesArray if its capacity is less than the desired capacity.

        Args:
            smps: SamplesArray stored in the Node
            cap (int): Desired capacity of the SamplesArray
        """
        smp_cap = len(smps)
        if smp_cap < cap:
            smps.grow(cap - smp_cap)

    @staticmethod
    def _resolve_range(
        start: Optional[int], stop: Optional[int], count: Optional[int]
    ) -> tuple[int, int, int]:
        """
        Resolve a range dependent on start, stop and count.
        At least two must be provided.

        Args:
            start (int): Desired start index
            stop (int): Desired stop index
            count (int): Desired span of the range

        Returns:
            tuple(start, stop, count)
        """
        provided = sum(i is not None for i in (start, stop, count))
        if provided == 1:
            raise ValueError("Two of start, stop, count must be provided")
        elif provided == 2:
            if start is None:
                start = stop - count
                if start < 0:
                    raise ValueError("Negative start index")
            elif stop is None:
                stop = start + count
            else:
                count = stop - start
        return start, stop, count

    def __init__(self, config, uuid: Optional[str] = None, size=0):
        """
        Initiallize a new node from config.

        Notes:
            - Capsule is available via self._hndle.
            - Capsule is available via self.config.
        """
        self.config = config
        self._smps = vn.smps_array(size)
        if uuid is None:
            self._hndle = vn.node_new(config, "")
        else:
            self._hndle = vn.node_new(config, uuid)

    def __del__(self):
        """
        Stop and delete a node if the class object is deleted.
        """
        vn.node_stop(self._hndle)
        vn.node_destroy(self._hndle)

    def __getitem__(self, idx: Union[int, slice]):
        """Return tuple containing self and index/slice for node operations."""
        if isinstance(idx, (int, slice)):
            return Node._SampleSlice(self, idx)
        else:
            logger.warning("Improper array index")
            raise ValueError("Improper Index")

    def __setitem__(self, obj):
        if isinstance(obj, Node):
            self.__del__()
            self.config = obj.config
            self._smps = obj._smps
        else:
            raise RuntimeError(f"{obj} is not of type `Node`")

    def __len__(self):
        return len(self._smps)

    def __copy__(self):
        """Disallow shallow copying."""
        raise RuntimeError("Copying Node is not allowed")

    def __deepcopy__(self):
        """Disallow deep copying"""
        raise RuntimeError("Copying a Node is not allowed")

    # bindings
    @staticmethod
    def memory_init(hugepages: int):
        """
        Initialize internal VILLASnode memory system.

        Args:
            hugepages (int): Amount of hugepages to be used.

        Notes:
            - Should be called once before any memory allocation is done.
            - Falls back to mmap if hugepages or root privilege unavailable.
        """
        return vn.memory_init(hugepages)

    def check(self):
        """Check node."""
        return vn.node_check(self._hndle)

    def details(self):
        """Get node details."""
        return vn.node_details(self._hndle)

    def input_signals_max_cnt(self):
        """Get max input signal count."""
        return vn.node_input_signals_max_cnt(self._hndle)

    def is_enabled(self):
        """Check whether or not node is enabled."""
        return vn.node_is_enabled(self._hndle)

    @staticmethod
    def is_valid_name(name: str):
        """Check if a name can be used for a node."""
        return vn.node_is_valid_name(name)

    def name(self):
        """Get node name."""
        return vn.node_name(self._hndle)

    def name_full(self):
        """Get node name with full details."""
        return vn.node_name_full(self._hndle)

    def name_short(self):
        """Get node name with less details."""
        return vn.node_name_short(self._hndle)

    def output_signals_max_cnt(self):
        """Get max output signal count."""
        return vn.node_output_signals_max_cnt(self._hndle)

    def pause(self):
        """Pause a node"""
        return vn.node_pause(self._hndle)

    def prepare(self):
        """Prepare a node"""
        return vn.node_prepare(self._hndle)

    @_warn_if_not_implemented
    def read_from(
        self,
        sample_length: int,
        cnt: Optional[int] = None,
        idx=None,
    ):
        """
        Read samples from a node into SamplesArray or block slice of samples.

        Args:
            sample_length (int): Length of each sample (number of signals).
            cnt (int): Number of samples to read.

        Returns:
            int: Number of samples read on success or -1.

        Notes:
            - Return value may vary depending on node type.
            - This function may be blocking.
        """
        if idx is None:
            if cnt is None:
                raise ValueError("Count is None")

            # resize _smps if too small
            Node._ensure_capacity(self._smps, cnt)

            # allocate new samples
            self._smps.bulk_alloc(0, len(self._smps), sample_length)

            return vn.node_read(self._hndle, self._smps.get_block(0), cnt)

        if isinstance(idx, int):
            if cnt is None:
                raise ValueError("Count is None")
            start = idx
            stop = start + cnt

            # if too small, resize _smps to match stop index
            Node._ensure_capacity(self._smps, stop)

            # allocate new samples
            self._smps.bulk_alloc(start, stop, sample_length)

            # read onward from index start
            return vn.node_read(self._hndle, self._smps.get_block(start), cnt)

        elif isinstance(idx, slice):
            start, stop, cnt = Node._resolve_range(idx.start, idx.stop, cnt)

            # check for length mismatch
            if (stop - start) != cnt:
                raise ValueError("Slice length and sample count do not match!")
            # if too small, resize _smps to match stop index
            Node._ensure_capacity(self._smps, stop)

            # allocate new samples
            self._smps.bulk_alloc(start, stop, sample_length)

            # read onward from index start
            return vn.node_read(self._hndle, self._smps.get_block(start), cnt)

        else:
            logger.warning("Invalid samples Parameter")
            return -1

    def restart(self):
        """Restart a node."""
        return vn.node_restart(self._hndle)

    def resume(self):
        """Resume a node."""
        return vn.node_resume(self._hndle)

    @_warn_if_not_implemented
    def reverse(self):
        """
        Reverse node input and output.

        Notes:
            - Hooks are not reversed.
            - Some nodes should be stopped or restarted before reversing.
            - Nodes with in-/output buffers should be stopped before reversing.
        """
        return vn.node_reverse(self._hndle)

    def start(self):
        """
        Start a node.

        Notes:
            - Nodes are not meant to be started again without stopping first.
        """
        return vn.node_start(self._hndle)

    def stop(self):
        """
        Stop a node.

        Notes:
            - Use before starting a node again.
            - May delete in-/output buffers of a node.
        """
        return vn.node_stop(self._hndle)

    def to_json(self):
        """
        Return the node configuration as json object.

        Notes:
            - Node configuration may not match self made configurations.
            - Node configuration does not contain node name.
        """
        json_str = vn.node_to_json_str(self._hndle)
        json_obj = json.loads(json_str)
        return json_obj

    def to_json_str(self):
        """
        Returns the node configuration as string.

        Notes:
            - Node configuration may not match self made configurations.
            - Node configuration does not contain node name.
        """
        return vn.node_to_json_str(self._hndle)

    @_warn_if_not_implemented
    def write_to(self, node, cnt: Optional[int] = None, idx=None):
        """
        Write samples from a SamplesArray fully or as block slice into a node.

        Args:
            node: Node handle.
            cnt (int): Number of samples to write.

        Returns:
            int: Number of samples written on success or -1.

        Notes:
            - Return value may vary depending on node type.
        """
        if idx is None:
            if cnt is None:
                raise ValueError("Count is None")

            return vn.node_write(self._hndle, node._smps.get_block(0), cnt)

        if isinstance(idx, int):
            if cnt is None:
                raise ValueError("Count is None")

            start = idx
            stop = start + cnt

            return vn.node_write(self._hndle, node._smps.get_block(start), cnt)

        if isinstance(idx, slice):
            start, stop, _ = idx.indices(len(self._smps))

            if cnt is None:
                cnt = stop - start

            print(start, stop, cnt, len(self._smps))

            # check for length mismatch
            if (stop - start) != cnt:
                raise ValueError("Slice length and sample count do not match.")
            # check for out of bounds
            if stop > len(self._smps):
                raise IndexError("Out of bounds")

            return vn.node_write(self._hndle, node._smps.get_block(start), cnt)

        logger.warning("Invalid samples Parameter")
        return -1

    def sample_length(self, idx: int):
        """Get the length of a sample."""
        if 0 <= idx and idx < len(self._smps):
            return vn.sample_length(self._smps[idx])
        else:
            raise IndexError(f"No Sample at index: {idx}")

    def pack_from(
        self,
        idx: int,
        values: Union[float, list[float], Capsule],
        ts_orig: Optional[int] = None,
        ts_recv: Optional[int] = None,
        seq: int = 0,
    ):
        """
        Packs a given sample from a source sample or value list.

        Args:
            idx (int): Node index to store packed sample in.
            ts_orig (Optional[int]): Supposed creation time in ns.
            ts_recv (Optional[int]): Supposed arrival time in ns.
            values (Union[float, list[float], sample]):
                - Packed sample will only hold referenced values.
            seq (int): supposed sequence number of the sample.
        """
        if seq < 0:
            raise ValueError("seq has to be positive")

        Node._ensure_capacity(self._smps, idx + 1)
        if len(self._smps) <= idx:
            self._smps.grow(idx + 1 - len(self._smps))

        if isinstance(values, (float, int)):
            self._smps[idx] = vn.sample_pack(
                [values],
                ts_orig,
                ts_recv,
                seq,
            )
        elif isinstance(values, list):
            self._smps[idx] = vn.sample_alloc(len(values))
            self._smps[idx] = vn.sample_pack(values, ts_orig, ts_recv, seq)
        else:  # assume a PyCapsule
            self._smps[idx] = vn.sample_pack(values, ts_orig, ts_recv)

    def unpack_to(
        self,
        r_idx: int,
        target_node,
        w_idx: int,
        ts_orig: Optional[int] = None,
        ts_recv: Optional[int] = None,
    ):
        """
        Unpacks a given sample to a destined target.

        Args:
            r_idx (int): Originating Node index to read from.
            ts_orig (Optional[int]): Supposed creation time in ns.
            ts_recv (Optional[int]): Supposed arrival time in ns.
            target_node (Node): Target node.
            w_idx (int): Target Node index to unpack to.
        """
        Node._ensure_capacity(self._smps, r_idx + 1)
        Node._ensure_capacity(target_node._smps, w_idx + 1)

        vn.sample_unpack(
            self._smps.get_block(r_idx),
            target_node._smps.get_block(w_idx),
            ts_orig,
            ts_recv,
        )

    def sample_details(self, idx):
        """
        Retrieve a dict with information about a sample.

        Keys:
            `sequence` (int): Sequence number of the sample.
            `length` (int): Sample length.
            `capacity` (int): Allocated sample length.
            `flags` (int): Number representing flags set of the sample.
            `refcnt` (int): Reference count of the given sample.
            `ts_origin` (float): Supposed timestamp of creation.
            `ts_received` (float): Supposed timestamp of arrival.

        Returns:
            Dict with listed keys and values.
        """
        return vn.sample_details(self._smps[idx])
