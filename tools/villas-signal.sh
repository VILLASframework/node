#!/bin/bash
# Wrapper around villas-pipe which uses the signal generator node-type
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

function usage() {
	echo "Usage: villas-signal [OPTIONS] SIGNAL"
	echo "  SIGNAL   is on of the following signal types:"
	echo "    mixed"
	echo "    random"
	echo "    sine"
	echo "    triangle"
	echo "    square"
	echo "    ramp"
	echo "    constants"
	echo "    counter"
	echo ""
	echo "  OPTIONS is one or more of the following options:"
	echo "    -d LVL  set debug level"
	echo "    -f FMT  set the format"
	echo "    -v NUM  specifies how many values a message should contain"
	echo "    -r HZ   how many messages per second"
	echo "    -n      non real-time mode. do not throttle output."
	echo "    -F HZ   the frequency of the signal"
	echo "    -a FLT  the amplitude"
	echo "    -D FLT  the standard deviation for 'random' signals"
	echo "    -o OFF  the DC bias"
	echo "    -l NUM  only send LIMIT messages and stop"
    echo
    echo "VILLASnode $(villas-node -v)"
    echo " Copyright 2014-2018, Institute for Automation of Complex Power Systems, EONERC"
    echo "  Steffen Vogel <StVogel@eonerc.rwth-aachen.de>"
    exit 1
}

OPTS=()

while getopts "v:r:f:l:a:D:no:h" OPT; do
    case $OPT in
        n) OPTS+=("-o realtime=false") ;;
		l) OPTS+=("-o limit=$OPTARG") ;;
        v) OPTS+=("-o values=$OPTARG") ;;
        r) OPTS+=("-o rate=$OPTARG") ;;
        o) OPTS+=("-o offset=$OPTARG") ;;
        f) OPTS+=("-o frequency=$OPTARG") ;;
        a) OPTS+=("-o amplitude=$OPTARG") ;;
        D) OPTS+=("-o stddev=$OPTARG") ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            usage
            exit 1
            ;;
    esac
done

# and shift them away
shift $((OPTIND - 1))

if (( $# != 1 )); then
    usage
    exit 1
fi

echo villas-pipe -r -t signal -o signal=$1 ${OPTS[@]}
