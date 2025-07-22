#!/usr/bin/env bash
#
# Run integration tests
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

SRCDIR=${SRCDIR:-$(realpath ${SCRIPTPATH}/..)}
BUILDDIR=${BUILDDIR:-${SRCDIR}/build}

LOGDIR=${BUILDDIR}/tests/integration
PATH=${BUILDDIR}/src:${SRCDIR}/tools:${PATH}

LANG=C

export PATH SRCDIR BUILDDIR LOGDIR LANG

# Default values
VERBOSE=${VERBOSE:-0}
FAIL_FAST=${FAIL_FAST:-0}
FILTER=${FILTER:-'*'}
NUM_SAMPLES=${NUM_SAMPLES:-100}
TIMEOUT=${TIMEOUT:-1m}

# Parse command line arguments
while getopts ":f:l:t:vg" OPT; do
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
        g)
             FAIL_FAST=1
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

PASSED=0
FAILED=0
SKIPPED=0
TIMEDOUT=0

# Preamble
echo -e "Starting integration tests for VILLASnode:\n"

for TEST in ${TESTS}; do
    TESTNAME=$(basename -s .sh ${TEST})

    # Start time measurement
    START=$(date +%s.%N)

    # Run test
    if (( ${VERBOSE} == 0 )); then
        timeout ${TIMEOUT} ${TEST} &> ${LOGDIR}/${TESTNAME}.log
        RC=$?
    else
        timeout ${TIMEOUT} ${TEST} | tee ${LOGDIR}/${TESTNAME}.log
        RC=${PIPESTATUS[0]}
    fi

    END=$(date +%s.%N)
    DIFF=$(echo "$END - $START" | bc)

    # Show full log in case of an error
    if (( ${VERBOSE} == 0 )); then
        if (( $RC != 99 )) && (( $RC != 0 )); then
             cat ${LOGDIR}/${TESTNAME}.log
        fi
    fi

    case $RC in
        0)
             echo -e "\e[32m[PASS] \e[39m ${TESTNAME} (ran for ${DIFF}s)"
             PASSED=$((${PASSED} + 1))
             ;;
        99)
             echo -e "\e[93m[SKIP] \e[39m ${TESTNAME}: $(head -n1 ${LOGDIR}/${TESTNAME}.log)"
             SKIPPED=$((${SKIPPED} + 1))
             ;;
        124)
             echo -e "\e[33m[TIME] \e[39m ${TESTNAME} (ran for more then ${TIMEOUT})"
             TIMEDOUT=$((${TIMEDOUT} + 1))
             FAILED=$((${FAILED} + 1))
             ;;
        *)
             echo -e "\e[31m[FAIL] \e[39m ${TESTNAME} (exited with code $RC, ran for ${DIFF}s)"
             FAILED=$((${FAILED} + 1))
             ;;
    esac

    TOTAL=$((${TOTAL} + 1))

    if (( ${RC} != 0 && ${RC} != 99 && ${FAIL_FAST} > 0 )); then
        break
    fi
done

# Show summary
if (( ${FAILED} > 0 )); then
    echo -e "\nSummary: ${FAILED} of ${TOTAL} tests failed."
    echo -e "   Timedout: ${TIMEDOUT}"
    echo -e "   Skipped: ${SKIPPED}"
    exit 1
else
    echo -e "\nSummary: all tests passed!"
    exit 0
fi
