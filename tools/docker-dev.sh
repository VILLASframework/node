#!/bin/bash
#
# Start a Docker based development environment
#
# @author Steffen Vogel <post@steffenvogel.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
##################################################################################

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

SRCDIR=${SRCDIR:-$(realpath ${SCRIPTPATH}/..)}
BUILDDIR=${BUILDDIR:-${SRCDIR}/build}

TAG=${TAG:-latest}
IMAGE="villas/node-dev:${TAG}"

docker run \
    --interactive \
    --tty \
    --publish 80:80 \
    --publish 443:443 \
    --publish 12000:12000/udp \
    --publish 12001:12001/udp \
    --publish 2345:2345 \
    --privileged \
    --security-opt seccomp:unconfined \
    --volume "${SRCDIR}:/villas" \
    ${IMAGE}
