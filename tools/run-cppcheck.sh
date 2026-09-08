#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

SOURCE_DIR=${SCRIPT_DIR}/..
BUILD_DIR=${SOURCE_DIR}/build

# Generate compilation database
cmake -S ${SOURCE_DIR} -B ${BUILD_DIR}

touch ${BUILD_DIR}/lib/formats/villas.pb-c.{c,h}

cppcheck -j $(nproc) \
    --max-configs=32 \
    --platform=unix64 \
    --error-exitcode=1 \
    --inline-suppr \
    --std=c++20 \
    --enable=warning,performance,portability \
    --suppressions-list=${SCRIPT_DIR}/cppcheck-supressions.txt \
    --project=${BUILD_DIR}/compile_commands.json \
    -D '__linux__' \
    -D '__x86_64__'
