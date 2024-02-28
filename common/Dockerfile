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
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLAScommon
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

FROM fedora:29

LABEL \
	org.label-schema.schema-version="1.0" \
	org.label-schema.name="VILLAScommon" \
	org.label-schema.license="GPL-3.0" \
	org.label-schema.vendor="Institute for Automation of Complex Power Systems, RWTH Aachen University" \
	org.label-schema.author.name="Steffen Vogel" \
	org.label-schema.author.email="stvogel@eonerc.rwth-aachen.de" \
	org.label-schema.description="A library for shared code across VILLAS C/C++ projects" \
	org.label-schema.url="http://fein-aachen.org/projects/villas-framework/" \
	org.label-schema.vcs-url="https://git.rwth-aachen.de/VILLASframework/VILLASfpga" \
	org.label-schema.usage="https://villas.fein-aachen.org/doc/"

# Toolchain
RUN dnf -y install \
	gcc gcc-c++ \
	pkgconfig make cmake \
	autoconf automake autogen libtool \
	texinfo git curl tar

# Several tools only needed for developement and testing
RUN dnf -y install \
	rpmdevtools rpm-build

# Some of the dependencies are only available in our own repo
ADD https://villas.fein-aachen.org/packages/villas.repo /etc/yum.repos.d/

# Dependencies
RUN dnf -y install \
	jansson-devel \
	libcurl-devel \
	libconfig-devel \
	openssl-devel openssl \
	spdlog-devel

# Build & Install Criterion
RUN cd /tmp && \
	git clone --recursive https://github.com/Snaipe/Criterion && \
	mkdir -p Criterion/build && cd Criterion/build && \
	cmake -DCMAKE_INSTALL_LIBDIR=/usr/local/lib64 .. && make install && \
	rm -rf /tmp/*

ENV LD_LIBRARY_PATH /usr/local/lib:/usr/local/lib64

WORKDIR /villas
ENTRYPOINT bash
