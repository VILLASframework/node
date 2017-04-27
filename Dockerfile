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

FROM fedora:latest
MAINTAINER Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

# Install dependencies
RUN dnf -y update && \
    dnf -y install \
	libconfig \
	libnl3 \
	libcurl \
	jansson

# Some additional tools required for running VILLASnode
RUN dnf -y update && \
    dnf -y install \
	iproute \
	openssl \
	kernel-modules-extra

# Install our own RPMs
COPY build/release/packaging/rpm/RPMS/ /rpms/
RUN rpm -i /rpms/x86_64/{libxil,libwebsockets,villas-node,villas-node-doc}-[0-9]*; rm -rf /rpms/

# For WebSocket / API access
EXPOSE 80
EXPOSE 443

ENTRYPOINT ["villas"]