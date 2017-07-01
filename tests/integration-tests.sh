#!/bin/bash
#
# Run integration tests
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

export SRCDIR=$(realpath ${SCRIPTPATH}/..)
export BUILDDIR=${SRCDIR}/build/release
export LOGDIR=${BUILDDIR}/tests/integration
export PATH=${BUILDDIR}:${PATH}

# Default values
VERBOSE=0
FILTER='*'

NUM_SAMPLES=100
TIMEOUT=1m

# Parse command line arguments
while getopts ":f:l:t:v" OPT; do
	case ${OPT} in
		f)
			FILTER=${OPTARG}
			;;
		v)
			VERBOSE=1
			;;
		l)
			NUM_SAMPLES=${OPTARG}
			;;
		t)
			TIMEOUT=${OPTARG}
			;;
		\?)
			echo "Invalid option: -${OPTARG}" >&2
			;;
		:)
			echo "Option -$OPTARG requires an argument." >&2
			exit 1
			;;
	esac
done

export VERBOSE
export NUM_SAMPLES

TESTS=${SRCDIR}/tests/integration/${FILTER}.sh

# Preperations
mkdir -p ${LOGDIR}

NUM_PASS=0
NUM_FAIL=0

# Preamble
echo -e "Starting integration tests for VILLASnode/fpga:\n"

for TEST in ${TESTS}; do
	TESTNAME=$(basename -s .sh ${TEST})

	# Run test
	if (( ${VERBOSE} == 0 )); then
		timeout ${TIMEOUT} ${TEST} &> ${LOGDIR}/${TESTNAME}.log
	else
		timeout ${TIMEOUT} ${TEST}
	fi
	RC=$?

	case $RC in
		0)
			echo -e "\e[32m[Pass] \e[39m ${TESTNAME}"
			NUM_PASS=$((${NUM_PASS} + 1))
			;;
		99)
			echo -e "\e[93m[Skip] \e[39m ${TESTNAME}"
			NUM_SKIP=$((${NUM_SKIP} + 1))
			;;
		124)
			echo -e "\e[31m[Timeout] \e[39m ${TESTNAME}"
			;;
		*)
			echo -e "\e[31m[Fail] \e[39m ${TESTNAME} with code $RC"
			NUM_FAIL=$((${NUM_FAIL} + 1))
			;;
	esac
done

# Show summary
if (( ${NUM_FAIL} > 0 )); then
	echo -e "\nSummary: ${NUM_FAIL} of $((${NUM_FAIL} + ${NUM_PASS})) tests failed."
	exit 1
else
	echo -e "\nSummary: all tests passed!"
	exit 0
fi