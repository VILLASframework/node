#!/bin/bash
# Wrapper to use make and villas commands with docker
#
# Run like this:
#   $ source tools/docker.sh
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

# In order to define aliases we need to source this script
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
	echo "Please use 'source' to load this script: $ source $0"
	exit
fi

DIR=$(realpath $(dirname $(realpath $BASH_SOURCE))/..)
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

DOCKER_IMAGE="villas/node-dev:${GIT_BRANCH}"

if [[ "$(docker images -q ${DOCKER_IMAGE} 2> /dev/null)" == "" ]]; then
	"make" docker-dev
fi

# Start container
docker run --rm --entrypoint bash --detach --tty --volume "${DIR}:/villas" ${DOCKER_IMAGE}

DOCKER="docker exec --tty --env PATH=\"/villas/build/release/:/usr/bin:/bin/\" $(docker ps --latest --quiet)"

# Then define alias for make and node
alias moby="${DOCKER}"
alias make="${DOCKER} make"
alias villas="${DOCKER} bash tools/villas.sh"
