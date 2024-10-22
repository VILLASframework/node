#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

cppcheck -j $(nproc) \
    --max-configs=32 \
    --error-exitcode=1 \
    --quiet \
    --inline-suppr \
    --enable=warning,performance,portability,missingInclude \
    --std=c++11 \
    --suppress=noValidConfiguration \
    -U '_MSC_VER;_WIN32;_M_ARM' \
    -U '_MSC_VER;_WIN32;_M_AMD64;_M_X64' \
    -U '_MSC_FULL_VER;_MSC_VER' \
    -U '_MSC_BUILD;_MSC_VER' \
    -I include \
    -I common/include \
    -I fpga/include \
    -I fpga/gpu/include \
    src/ \
    lib/ \
    common/lib/ \
    common/tests/unit/ \
    tests/unit/ \
    fpga/gpu/src \
    fpga/gpu/lib \
    fpga/src/ \
    fpga/lib/ \
    fpga/tests/unit/
