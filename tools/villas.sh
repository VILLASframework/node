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
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

PREFIX=villas

# Get a list of all available tools
TOOLS=$(compgen -c | egrep "^$PREFIX-" | sort -u | cut -d- -f2- | paste -sd'|')

# First argument to wrapper is the tool which should be started
TOOL=$1

# Following arguments will be passed directly to tool
ARGS=${@:2}

# Check if tool is available
if ! [[ "$TOOL" =~ $(echo ^\($TOOLS\)$) ]]; then
	echo "Usage: villas [TOOL]" 1>&2
	echo "  TOOL     is one of ${TOOLS}"
	echo
	echo "For detailed documentation, please run 'villas node'"
	echo " and point your web browser to http://localhost:80"
	echo
	# Show VILLASnode copyright and contact info
	villas-node -h | tail -n3
	exit 1
fi

exec $PREFIX-$TOOL $ARGS
