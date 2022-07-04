#!/bin/bash
#
# Some helper functions for our integration test suite
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
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
