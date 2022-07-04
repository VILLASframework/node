# Dockerfile for creating VILLAS live images
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2017-2022, Institute for Automation of Complex Power Systems, EONERC
# @license Apache 2.0
###################################################################################

FROM fedora:35

RUN dnf -y install \
	git \
	lorax \
	rclone \
	make \
	wget \
	lorax-lmc-novirt \
	lorax-lmc-virt \
	livecd-tools
