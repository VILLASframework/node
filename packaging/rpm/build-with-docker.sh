#!/bin/bash

# Build VILLASnode RPM package for OPAL-RTLinux using Docker images
#
# Author: Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-FileCopyrightText: 2023-2024 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

#
# WARNING: This script requires access to OPAL-RTs Gitlab in order to use OPAL-RTLinux Docker images!
#

RTLAB_VERSION=${RTLAB_VERSION-"2023.3"}
ORTLINUX_VERSION=${ORTLINUX_VERSION-"3.5.4"}

DOCKER_IMAGE=${DOCKER_IMAGE-"gitlab.opal-rt.com:52520/opal-rt-dockerimages/docker-opalrtlinux3-ci:release-${ORTLINUX_VERSION}-1.0.0"}

RTLAB_DIR=${RTLAB_DIR-"C:\\OPAL-RT\\RT-LAB\\${RTLAB_VERSION}"}

SOURCE_DIR=$(realpath $(dirname "${BASH_SOURCE:-$0}")/../..)

export MSYS_NO_PATHCONV=1 # For Windows compatability

docker run \
    --volume "${RTLAB_DIR}:/rtlab" \
    --volume "${SOURCE_DIR}:/src" \
    --workdir /src \
    --entrypoint /bin/bash \
    ${DOCKER_IMAGE} \
    -- /src/packaging/rpm/build.sh
