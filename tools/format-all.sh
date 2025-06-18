#!/usr/bin/env bash
#
# Format all C++ files in repo
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

git ls-files -c -z -- "*.c" ".h" "*.hpp" "*.cpp" ":!:fpga/thirdparty" |\
    xargs -0 clang-format --verbose -i
