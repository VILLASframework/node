#!/bin/bash
# Wrapper to start tool of the S2SS suite
#
# This allows you to use S2SS tools like this:
#    $ s2ss server /etc/cosima/test.cfg
#
# Install by:
#    $ make install
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
#   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
#   Unauthorized copying of this file, via any medium is strictly prohibited.
#
##################################################################################

PREFIX=s2ss

# Get a list of all available tools
TOOLS=$(compgen -c | egrep "^$PREFIX-" | sort | cut -d- -f2 | paste -sd\|)

# First argument to wrapper is the tool which should be started
TOOL=$1

# Following arguments will be passed directly to tool
ARGS=${@:2}

# Check if tool is available
if ! [[ "$TOOL" =~ $(echo ^\($TOOLS\)$) ]]; then
	echo "Usage s2ss ($TOOLS)" 1>&2
	exit 1
fi

exec $PREFIX-$TOOL $ARGS