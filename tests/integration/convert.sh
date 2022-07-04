#!/bin/bash
#
# Integration test for villas convert tool
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
	popd
	rm -rf ${DIR}
}
trap finish EXIT

FORMATS="villas.human csv tsv json opal.asyncip"

villas signal -v5 -n -l20 mixed > input.dat

for FORMAT in ${FORMATS}; do
	villas convert -o ${FORMAT} < input.dat | tee ${TEMP} | \
	villas convert -i ${FORMAT} > output.dat

	CMP_FLAGS=""
	if [ ${FORMAT} = "opal.asyncip" ]; then
		CMP_FLAGS+=-T
	fi

	villas compare ${CMP_FLAGS} input.dat output.dat
done
