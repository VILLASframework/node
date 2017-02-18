# Dockerfile for VILLASnode development and testing
#
# Use this Dockerfile running:
#    $ make docker
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
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
	libcurl-devel \
	jansson-devel

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

# Tools for coverage and profiling
RUN dnf -y update && \
    dnf -y install \
        python-pip && \
    pip install \
        gcovr

ENV PKG_CONFIG_PATH /usr/local/lib/pkgconfig
ENV LD_LIBRARY_PATH /usr/local/lib

# Build & Install libxil
COPY thirdparty/libxil /tmp/libxil
RUN mkdir -p /tmp/libxil/build && cd /tmp/libxil/build && cmake .. && make install

# Build & Install Criterion
COPY thirdparty/criterion /tmp/criterion
RUN mkdir -p /tmp/criterion/build && cd /tmp/criterion/build && cmake .. && make install

# Build & Install libwebsockets
COPY thirdparty/libwebsockets /tmp/libwebsockets
RUN mkdir -p /tmp/libwebsockets/build && cd /tmp/libwebsockets/build && cmake .. && make install

# Cleanup intermediate files from builds
RUN rm -rf /tmp/*

WORKDIR /villas

ENTRYPOINT make clean && make install && villas node; bash
