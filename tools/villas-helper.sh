#!/bin/bash
#
# Some helper functions for our integration test suite
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
##################################################################################

function villas_format_supports_vectorize() {
	local FORMAT=$1
	
	case ${FORMAT} in
		raw*) return 1 ;;
		gtnet*) return 1 ;;
	esac
	
	return 0
}

function villas_format_supports_header() {
	local FORMAT=$1

	case $FORMAT in
		raw*) return 1 ;;
		gtnet) return 1 ;;
	esac
	
	return 0
}

function colorize() {
	RANDOM=${BASHPID}
	echo -e "\x1b[0;$((31 + ${RANDOM} % 7))m$1\x1b[0m"
}

function villas() {
	VILLAS_LOG_PREFIX=${VILLAS_LOG_PREFIX:-$(colorize "[$1-$((${RANDOM} % 100))} ")} \
	command villas $@
}
