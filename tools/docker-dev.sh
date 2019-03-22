#!/bin/bash
#
# Start a Docker based development environment
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
##################################################################################

SCRIPT=$(realpath ${BASH_SOURCE[0]})
SCRIPTPATH=$(dirname $SCRIPT)

SRCDIR=${SRCDIR:-$(realpath ${SCRIPTPATH}/..)}
BUILDDIR=${BUILDDIR:-${SRCDIR}/build}

TAG=${TAG:-develop}
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
