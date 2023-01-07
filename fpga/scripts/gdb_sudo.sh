#!/bin/bash
#
# Run GDB as sudo for VS Code debugger
# See: .vscode directory
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017-2022, Institute for Automation of Complex Power Systems, EONERC
# SPDX-License-Identifier: Apache-2.0
##################################################################################

sudo pkexec /usr/bin/gdb "$@"
