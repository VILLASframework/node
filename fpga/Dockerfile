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
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASfpga
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

FROM nvidia/cuda:9.2-devel-centos7

LABEL \
	org.label-schema.schema-version="1.0" \
	org.label-schema.name="VILLASfpga" \
	org.label-schema.license="GPL-3.0" \
	org.label-schema.vendor="Institute for Automation of Complex Power Systems, RWTH Aachen University" \
	org.label-schema.author.name="Steffen Vogel" \
	org.label-schema.author.email="stvogel@eonerc.rwth-aachen.de" \
	org.label-schema.description="A image containing all build-time dependencies for VILLASfpga based on Fedora" \
	org.label-schema.url="http://fein-aachen.org/projects/villas-framework/" \
	org.label-schema.vcs-url="https://git.rwth-aachen.de/VILLASframework/VILLASfpga" \
	org.label-schema.usage="https://villas.fein-aachen.org/doc/fpga.html"

# Enable Extra Packages for Enterprise Linux (EPEL) and Software collection repo
RUN yum -y install epel-release centos-release-scl

# Toolchain
RUN yum -y install \
	devtoolset-7-toolchain \
	pkgconfig make cmake3 \
	autoconf automake autogen libtool \
	texinfo git curl tar

# Several tools only needed for developement and testing
RUN yum -y install \
	rpmdevtools rpm-build

# Dependencies
RUN yum -y install \
	jansson-devel \
	openssl-devel \
	curl-devel \
	lapack-devel

# Build & Install Fmtlib
RUN git clone --recursive https://github.com/fmtlib/fmt.git /tmp/fmt && \
    mkdir -p /tmp/fmt/build && cd /tmp/fmt/build && \
    git checkout 5.2.0 && \
    cmake3 -DBUILD_SHARED_LIBS=1 .. && \
    make -j$(nproc) install && \
	rm -rf /tmp/fmt

# Build & Install spdlog
RUN git clone --recursive https://github.com/gabime/spdlog.git /tmp/spdlog && \
    mkdir -p /tmp/spdlog/build && cd /tmp/spdlog/build && \
    git checkout v1.3.1 && \
    cmake3 -DSPDLOG_FMT_EXTERNAL=ON -DSPDLOG_BUILD_BENCH=OFF .. && \
    make -j$(nproc) install && \
	rm -rf /tmp/spdlog

# Build & Install Criterion
RUN git clone --recursive https://github.com/Snaipe/Criterion /tmp/criterion && \
    mkdir -p /tmp/criterion/build && cd /tmp/criterion/build && \
    git checkout v2.3.3 && \
	cmake3 .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/*

# Build & Install libxil
RUN git clone https://git.rwth-aachen.de/acs/public/villas/fpga/libxil.git /tmp/libxil && \
	mkdir -p /tmp/libxil/build && cd /tmp/libxil/build && \
	cmake3 .. && \
	make -j$(nproc) install && \
	rm -rf /tmp/*

ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib64

WORKDIR /fpga
ENTRYPOINT scl enable devtoolset-7 bash
