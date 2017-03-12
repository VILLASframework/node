#!/bin/bash
# Wrapper to start tool of the VILLASnode suite
#
# This allows you to use VILLASnode tools like this:
#    $ villas node /etc/villas/test.cfg
#
# Install by:
#    $ make install
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
###################################################################################

PREFIX=villas

# Get a list of all available tools
TOOLS=$(compgen -c | egrep "^$PREFIX-" | sort | cut -d- -f2- | paste -sd\|)

# First argument to wrapper is the tool which should be started
TOOL=$1

# Following arguments will be passed directly to tool
ARGS=${@:2}

# Check if tool is available
if ! [[ "$TOOL" =~ $(echo ^\($TOOLS\)$) ]]; then
	echo "Usage villas ($TOOLS)" 1>&2
	exit 1
fi

exec $PREFIX-$TOOL $ARGS