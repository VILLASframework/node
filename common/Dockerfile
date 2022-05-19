# Dockerfile for VILLAScommon development.
#
# This Dockerfile builds an image which contains all library dependencies
# and tools to build VILLAScommon.
# However, VILLAScommon itself it not part of the image.
#
# This image can be used for developing VILLAScommon
# by running:
#   make docker
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache License 2.0
#
# VILLAScommon
#
###################################################################################

FROM fedora:33

LABEL \
	org.label-schema.schema-version="1.0" \
	org.label-schema.name="VILLAScommon" \
	org.label-schema.license="Apache-2.0" \
	org.label-schema.vendor="Institute for Automation of Complex Power Systems, RWTH Aachen University" \
	org.label-schema.author.name="Steffen Vogel" \
	org.label-schema.author.email="svogel2@eonerc.rwth-aachen.de" \
	org.label-schema.description="A library for shared code across VILLAS C/C++ projects" \
	org.label-schema.url="http://fein-aachen.org/projects/villas-framework/" \
	org.label-schema.vcs-url="https://git.rwth-aachen.de/VILLASframework/VILLASfpga" \
	org.label-schema.usage="https://villas.fein-aachen.org/doc/"

# Toolchain
RUN dnf -y install \
	gcc gcc-c++ \
	make cmake \
	cppcheck \
	pkgconfig git curl tar

# Dependencies
RUN dnf -y install \
	jansson-devel \
	libcurl-devel \
	libconfig-devel \
	libuuid-devel \
	spdlog-devel \
	fmt-devel \
	openssl-devel openssl

# Build & Install Criterion
RUN cd /tmp && \
	git clone --recursive https://github.com/Snaipe/Criterion && \
	mkdir -p Criterion/build && cd Criterion/build && \
	git checkout v2.3.3 && \
	cmake -DCMAKE_INSTALL_LIBDIR=/usr/local/lib64 .. && make install && \
	rm -rf /tmp/*

ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib64

WORKDIR /villas
