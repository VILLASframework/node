# Dockerfile for VILLASnode dependencies.
#
# This Dockerfile builds an image which contains all library dependencies
# and tools to build VILLASnode.
# However, VILLASnode itself it not part of the image.
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
###################################################################################

ARG BUILDER_IMAGE=villas/node-dev

# This image is built by villas-node-git/packaging/docker/Dockerfile.dev
FROM villas/node-dev:develop AS builder

COPY . /villas-node/

RUN rm -rf /villas-node/build
WORKDIR /villas-node

#RUN dnf -y install

RUN make -j2 rpm-villas-node

FROM fedora:28

ARG GIT_REV=unkown
ARG GIT_BRANCH=unkown
ARG VERSION=unkown
ARG VARIANT=unkown
	
# Some of the dependencies are only available in our own repo
ADD https://packages.fein-aachen.org/redhat/fein.repo /etc/yum.repos.d/

# Usually the following dependecies would be resolved by dnf
# when installing villas-node.
# We add them here to utilise Dockers caching and layer feature
# in order reduce bandwidth and space usage.
RUN dnf -y install \
	openssl \
	libconfig \
	libnl3 \
	libcurl \
	jansson \
	iproute \
	module-init-tools

ADD https://villas.fein-aachen.org/packages/villas.repo /etc/yum.repos.d/

COPY --from=builder /villas-node/build/Linux-x86_64-release/packaging/rpm/RPMS/x86_64/*.rpm /tmp/
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
