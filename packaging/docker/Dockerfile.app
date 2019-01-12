# Dockerfile
#
# This image can be used for running VILLASnode
# by running:
#   make docker
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

ARG BUILDER_IMAGE=villas/node-dev
ARG DOCKER_TAG=latest
ARG GIT_REV=unknown
ARG GIT_BRANCH=unknown
ARG VERSION=unknown
ARG VARIANT=unknown

# This image is built by villas-node-git/packaging/docker/Dockerfile.dev
FROM $BUILDER_IMAGE AS builder

COPY . /villas/

RUN rm -rf /villas/build && mkdir /villas/build
WORKDIR /villas/build
RUN cmake -DCPACK_GENERATOR=RPM ..
RUN make -j$(nproc) doc
RUN make -j$(nproc) package

FROM fedora:29
	
# Some of the dependencies are only available in our own repo
ADD https://packages.fein-aachen.org/fedora/fein.repo /etc/yum.repos.d/

COPY --from=builder /villas/build/*.rpm /tmp/
RUN dnf -y install /tmp/*.rpm

# For WebSocket / API access
EXPOSE 80
EXPOSE 443

ENTRYPOINT ["villas"]

LABEL \
	org.label-schema.schema-version = "1.0" \
	org.label-schema.name = "VILLASnode" \
	org.label-schema.license = "GPL-3.0" \
	org.label-schema.vcs-ref="$GIT_REV" \
	org.label-schema.vcs-branch="$GIT_BRANCH" \
	org.label-schema.version="$VERSION" \
	org.label-schema.variant="$VARIANT" \
	org.label-schema.vendor = "Institute for Automation of Complex Power Systems, RWTH Aachen University" \
	org.label-schema.author.name = "Steffen Vogel" \
	org.label-schema.author.email = "stvogel@eonerc.rwth-aachen.de" \
	org.label-schema.description = "A image containing for VILLASnode based on Fedora" \
	org.label-schema.url = "http://fein-aachen.org/projects/villas-framework/" \
	org.label-schema.vcs-url = "https://git.rwth-aachen.de/VILLASframework/VILLASnode" \
	org.label-schema.usage = "https://villas.fein-aachen.org/doc/node-installation.html#node-installation-docker"
