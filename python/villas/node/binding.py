"""
Author: Kevin Vu te Laar <vu.te@rwth-aachen.de>
SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
SPDX-License-Identifier: Apache-2.0
"""  # noqa: E501

from typing import Optional, Union

import functools
import json
import logging

import villas_node as vn

logger = logging.getLogger("villas.node")


class SamplesArray:
    """
    Wrapper for a block of samples with automatic memory management.

    Supports:
        - Reading block slices in combination with node_read().
        - Writing block slices in combination with node_write().
        - Automatic (de-)allocation of samples.

    Notes:
        - Block slices are a slices with step size 1
    """

    def _bulk_allocation(self, start_idx: int, end_idx: int, smpl_length: int):
        """
        Allocates a block of samples.

        Args:
            start_idx (int): Starting Index of a block.
            end_idx (int): One past the last index to allocate.
            smpl_length (int): length of sample per slot

        Notes:
            - Block is determined by `start_idx`, `end_idx`
            - A sample can hold multiple signals. smpl_length corresponds to the number of signals.
        """
        return self._smps.bulk_alloc(start_idx, end_idx, smpl_length)

    def _get_block_handle(self, start_idx: Optional[int] = None):
        """
        Get a handle to a block of samples.

        Args:
            start_idx (Optional[int]): Starting index of the block. Defaults to 0.

        Returns:
            vsample**: Pointer/handle to the underlying block masked as `void *`.
        """
        if start_idx is None:
            return self._smps.get_block(0)
        else:
            return self._smps.get_block(start_idx)

    def __init__(self, length: int):
        """
        Initialize a SamplesArray.

        Notes:
            Each sample slot can hold one node::Sample allocated via node_read().
            node::Sample can contain multiple signals, depending on smpl_length.
        """
        self._smps = vn.smps_array(length)
        self._len = length

    def __len__(self):
        """Returns the length of the SamplesArray, which corresponds to the amount of samples it holds."""
        return self._len

    def __getitem__(self, idx: Union[int, slice]):
        """Return a tuple containing self and an index or slice for node operations."""
        if isinstance(idx, slice):
            return (self, idx)
        elif isinstance(idx, int):
            return (self, idx)
        else:
            logger.warning("Improper array index")
            raise ValueError("Improper Index")

    def __copy__(self):
        """Disallow shallow copying."""
        raise RuntimeError("Copying SamplesArray is not allowed")

    def __deepcopy__(self):
        """Disallow deep copying."""
        raise RuntimeError("Copying SamplesArray is not allowed")


# helper functions


# function decorator for optional node_compat function calls
#   that would return -1 if a function is not implemented
def _warn_if_not_implemented(func):
    """
    Decorator to warn if specific node_* functions are not implemented and return -1.

    Returns:
        Wrapping function that logs a warning if the return value is -1.
    """

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        ret = func(*args, **kwargs)
        if ret == -1:
            msg = f"[\033[33mWarning\033[0m]: Function '{func.__name__}()' is not implemented for node type '{vn.node_name(*args)}'."
            logger.warning(msg)
        return ret

    return wrapper


# node API bindings


def memory_init(*args):
    return vn.memory_init(*args)


def node_check(node):
    """Check node."""
    return vn.node_check(node)


def node_destroy(node):
    """
    Delete a node.

    Notes:
        - Note should be stopped first.
    """
    return vn.node_destroy(node)


def node_details(node):
    """Get node details."""
    return vn.node_details(node)


def node_input_signals_max_cnt(node):
    """Get max input signal count."""
    return vn.node_input_signals_max_cnt(node)


def node_is_enabled(node):
    """Check whether or not node is enabled."""
    return vn.node_is_enabled(node)


def node_is_valid_name(name: str):
    """Check if a name can be used for a node."""
    return vn.node_is_valid_name(name)


def node_name(node):
    """Get node name."""
    return vn.node_name(node)


def node_name_full(node):
    """Get node name with full details."""
    return vn.node_name_full(node)


def node_name_short(node):
    """Get node name with less details."""
    return vn.node_name_short(node)


def node_netem_fds(*args):
    return vn.node_netem_fds(*args)


def node_new(config, uuid: Optional[str] = None):
    """
    Create a new node.

    Args:
        config (json/str): Configuration of the node.
        uuid (Optional[str]): Unique identifier of the node. If `None`/empty, VILLASnode will assign one by default.

    Returns:
        vnode *: Handle to a node.
    """
    if uuid is None:
        return vn.node_new(config, "")
    else:
        return vn.node_new(config, uuid)


def node_output_signals_max_cnt(node):
    return vn.node_output_signals_max_cnt(node)


def node_pause(node):
    """Pause a node"""
    return vn.node_pause(node)


def node_poll_fds(*args):
    return vn.node_poll_fds(*args)


def node_prepare(node):
    """Prepare a node"""
    return vn.node_prepare(node)


@_warn_if_not_implemented
def node_read(node, samples, sample_length, count):
    """
    Read samples from a node into a SamplesArray or a block slice of its samples.

    Args:
        node: Node handle.
        samples: Either a SamplesArray or a tuple of (SamplesArray, index/slice).
        sample_length: Length of each sample (number of signals).
        count: Number of samples to read.

    Returns:
        int: Number of samples read on success or -1 if not implemented.

    Notes:
        - Return value may vary depending on node type.
        - This function may be blocking.
    """
    if isinstance(samples, SamplesArray):
        samples._bulk_allocation(0, len(samples), sample_length)
        return vn.node_read(node, samples._get_block_handle(0), count)
    elif isinstance(samples, tuple):
        smpls = samples[0]
        if not isinstance(samples[1], slice):
            raise ValueError("Invalid samples Parameter")
        start, stop, _ = samples[1].indices(len(smpls))

        # check for length mismatch
        if (stop - start) != count:
            raise ValueError("Sample slice length and sample count do not match.")
        # check if out of bounds
        if stop > len(smpls):
            raise IndexError("Out of bounds")

        # allocate new samples and get block handle
        samples[0]._bulk_allocation(start, stop, sample_length)
        handle = samples[0]._get_block_handle(start)

        return vn.node_read(node, handle, count)
    else:
        logger.warning("Invalid samples Parameter")
        return -1


def node_restart(node):
    """Restart a node."""
    return vn.node_restart(node)


def node_resume(node):
    """Resume a node."""
    return vn.node_resume(node)


@_warn_if_not_implemented
def node_reverse(node):
    """
    Reverse node input and output.

    Notes:
        - Hooks are not reversed.
        - Some nodes should be stopped or restarted before reversing. Especially nodes with in-/output buffers.
    """
    return vn.node_reverse(node)


def node_start(node):
    """
    Start a node.

    Notes:
        - Nodes are not meant to be started again without stopping first.
    """
    return vn.node_start(node)


def node_stop(node):
    """
    Stop a node.

    Notes:
        - Use before starting a node again.
    """
    return vn.node_stop(node)


def node_to_json(node):
    """
    Return the node configuration as json object.

    Notes:
        - Node configuration may not match self made configurations.
        - Node configuration does not contain node name.
    """
    json_str = vn.node_to_json_str(node)
    json_obj = json.loads(json_str)
    return json_obj


def node_to_json_str(node):
    """
    Returns the node configuration as string.

    Notes:
        - Node configuration may not match self made configurations.
        - Node configuration does not contain node name.
    """
    return vn.node_to_json_str(node)


@_warn_if_not_implemented
def node_write(node, samples, count):
    """
    Write samples from a SamplesArray, fully or as block slice, into a node.

    Args:
        node: Node handle.
        samples: Either a SamplesArray or a tuple of (SamplesArray, index/slice).
        count: Number of samples to write.

    Returns:
        int: Number of samples written on success, or -1 if not implemented.

    Notes:
        - Return value may vary depending on node type.
    """
    if isinstance(samples, SamplesArray):
        return vn.node_write(node, samples._get_block_handle(), count)
    elif isinstance(samples, tuple):
        smpls = samples[0]
        if not isinstance(samples[1], slice):
            raise ValueError("Invalid samples Parameter")
        start, stop, _ = samples[1].indices(len(smpls))

        # check for length mismatch
        if (stop - start) != count:
            raise ValueError("Sample slice length and sample count do not match.")
        # check for out of bounds
        if stop > len(smpls):
            raise IndexError("Out of bounds")

        # get block handle
        handle = samples[0]._get_block_handle(start)

        return vn.node_write(node, handle, count)
    else:
        logger.warning("Invalid samples Parameter")


def sample_length(*args):
    vn.sample_length(*args)


def sample_pack(*args):
    vn.sample_pack(*args)


def sample_unpack(*args):
    vn.sample_unpack(*args)
