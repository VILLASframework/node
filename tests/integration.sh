#!/bin/bash

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

TOPDIR=$(realpath ${SCRIPTPATH}/..)
BUILDDIR=${TOPDIR}/build/release
PATH=${BUILDDIR}:${PATH}

CONFIGFILE=$(mktemp)
DATAFILE_ORIG=$(mktemp)
DATAFILE_LOOP=$(mktemp)

cat > ${CONFIGFILE} <<- EOM
nodes = {
	node1 = {
		type = "socket";
		layer = "udp";
		
		local = "*:12000";
		remote = "localhost:12000"
	}
}
EOM

# Generate test data
villas-signal random -l 10 -r 100 > ${DATAFILE_ORIG}

villas-pipe ${CONFIGFILE} node1 < ${DATAFILE_ORIG} > ${DATAFILE_LOOP}

diff -u ${DATAFILE_ORIG} ${DATAFILE_LOOP}

rm ${DATAFILE_ORIG}
rm ${DATAFILE_LOOP}
rm ${CONFIGFILE}