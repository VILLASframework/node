# Dockerfile for VILLASnode development and testing
#
# Use this Dockerfile running:
#    $ make docker
#
# @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
#   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
#   Unauthorized copying of this file, via any medium is strictly prohibited. 
###################################################################################

FROM fedora:latest
MAINTAINER Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

# Expose ports for HTTP and WebSocket frontend
EXPOSE 80
EXPOSE 443

# Toolchain & dependencies
RUN dnf -y update && \
    dnf -y install \
	pkgconfig \
	gcc \
	make \
	wget \
	tar \
	cmake \
	openssl-devel \
	libconfig-devel \
	libnl3-devel \
	pciutils-devel \
	libcurl-devel \
	jansson-devel \
	libuuid-devel

# Tools for documentation
RUN dnf -y update && \
    dnf -y install \
	doxygen \
	dia \
	graphviz

# Tools for deployment / packaging
RUN dnf -y update && \
    dnf -y install \
	openssh-clients \
	rpmdevtools \
	rpm-build

# Tools to build dependencies
RUN dnf -y update && \
    dnf -y install \
	git \
	gcc-c++ \
	autoconf \
	automake \
	autogen \
	libtool \
	flex \
	bison \
	texinfo
	

ENTRYPOINT /bin/bash
