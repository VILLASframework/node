# Dockerfile for creating VILLAS live images
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2017-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

FROM fedora:36

RUN dnf -y install \
	git \
	lorax \
	rclone \
	make \
	wget \
	lorax-lmc-novirt \
	lorax-lmc-virt \
	livecd-tools
