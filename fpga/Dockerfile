# Dockerfile for VILLASfpga development.
#
# This Dockerfile builds an image which contains all library dependencies
# and tools to build VILLASfpga.
# However, VILLASfpga itself it not part of the image.
#
# This image can be used for developing VILLASfpga
# by running:
#   make docker
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

FROM rockylinux:9

LABEL \
	org.label-schema.schema-version="1.0" \
	org.label-schema.name="VILLASfpga" \
	org.label-schema.license="Apache-2.0" \
	org.label-schema.vendor="Institute for Automation of Complex Power Systems, RWTH Aachen University" \
	org.label-schema.author.name="Steffen Vogel" \
	org.label-schema.author.email="post@steffenvogel.de" \
	org.label-schema.description="A image containing all build-time dependencies for VILLASfpga based on Fedora" \
	org.label-schema.url="http://fein-aachen.org/projects/villas-framework/" \
	org.label-schema.vcs-url="https://git.rwth-aachen.de/VILLASframework/VILLASfpga" \
	org.label-schema.usage="https://villas.fein-aachen.org/doc/fpga.html"

# Enable Extra Packages for Enterprise Linux (EPEL) and Software collection repo
RUN dnf -y update && \
    dnf install -y epel-release dnf-plugins-core && \
    dnf install -y https://rpms.remirepo.net/enterprise/remi-release-9.rpm && \
    dnf config-manager --set-enabled crb && \
    dnf config-manager --set-enabled remi

# Toolchain
RUN dnf -y install \
	git clang gdb ccache \
	redhat-rpm-config \
	rpmdevtools rpm-build\
	make cmake ninja-build \
    wget \
    pkgconfig \
    autoconf automake libtool \
    cppcheck \
    git curl tar

# Dependencies
RUN dnf -y install \
	jansson-devel \
	openssl-devel \
	curl-devel \
	lapack-devel \
	libuuid-devel

# Build & Install Fmtlib
RUN git clone --recursive https://github.com/fmtlib/fmt.git /tmp/fmt && \
	mkdir -p /tmp/fmt/build && cd /tmp/fmt/build && \
	git checkout 6.1.2 && \
	cmake3 -DBUILD_SHARED_LIBS=1 -DFMT_TEST=OFF .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/fmt

# Build & Install spdlog
RUN git clone --recursive https://github.com/gabime/spdlog.git /tmp/spdlog && \
	mkdir -p /tmp/spdlog/build && cd /tmp/spdlog/build && \
	git checkout v1.8.2 && \
	cmake -DSPDLOG_FMT_EXTERNAL=ON \
        -DSPDLOG_BUILD_BENCH=OFF \
        -DSPDLOG_BUILD_SHARED=ON \
        -DSPDLOG_BUILD_TESTS=OFF .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/spdlog

# Build & Install Criterion
RUN git clone --recursive https://github.com/Snaipe/Criterion /tmp/criterion && \
	mkdir -p /tmp/criterion/build && cd /tmp/criterion/build && \
	git checkout v2.3.3 && \
	cmake .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/*

# Build & Install libxil
RUN git clone https://git.rwth-aachen.de/acs/public/villas/fpga/libxil.git /tmp/libxil && \
	mkdir -p /tmp/libxil/build && cd /tmp/libxil/build && \
	cmake .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/*

ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib64

WORKDIR /fpga

