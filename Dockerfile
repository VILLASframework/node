# Dockerfile for VILLASnode dependencies.
#
# This Dockerfile builds an image which contains all library dependencies
# and tools to build VILLASnode.
# However, VILLASnode itself it not part of the image.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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