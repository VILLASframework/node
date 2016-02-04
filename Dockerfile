# Dockerfile for S2SS development and testing
#
# Use this Dockerfile running:
#    $ make docker
#
# @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
#   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
#   Unauthorized copying of this file, via any medium is strictly prohibited. 
###################################################################################

FROM stv0g/dotfiles
#FROM debian:jessie

MAINTAINER Steffen Vogel <stvogel@eonerc.rwth-aachen.de>

# Expose ports for HTTP and WebSocket frontend
EXPOSE 80
EXPOSE 443

# Install development environement
RUN apt-get update && \
    apt-get -y install \
        gcc \
	g++ \
	clang \
	gdb \
	bison \
	flex \
	make \
	cmake \
	libc6-dev \
        pkg-config \
	doxygen \
	dia \
	graphviz \
	wget \
	vim

# Install dependencies for native arch
RUN apt-get update && \
    apt-get -y install \
        libconfig-dev \
        libnl-3-dev \
        libnl-route-3-dev \
        libpci-dev \
        libjansson-dev \
        libcurl4-openssl-dev \
	libssl-dev \
        uuid-dev

# Install dependencies for 32bit x86 arch (required for 32bit libOpalAsync)
#  (64 bit header files are used)
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get -y install \
	libc6-dev-i386 \
	lib32gcc-4.9-dev \
        libconfig9:i386 \
        libnl-3-200:i386 \
        libnl-route-3-200:i386 \
        libpci3:i386 \
        libjansson4:i386 \
        libcurl3:i386 \
        libuuid1:i386

# Checkout source code from GitHub
#RUN git clone git@github.com:RWTH-ACS/S2SS.git /s2ss

WORKDIR /s2ss

# Compile
#RUN make -C /s2ss

ENTRYPOINT /bin/bash