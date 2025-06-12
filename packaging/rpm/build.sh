#!/bin/bash

# Build VILLASnode RPM package
#
# Author: Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-FileCopyrightText: 2023-2024 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

RTLAB_DIR=${RTLAB_DIR-/rtlab}
SOURCE_DIR=${SOURCE_DIR-$(realpath $(dirname "${BASH_SOURCE:-$0}")/../..)}

# Quirks for OPAL-RTLinux & Docker Images
if [ $(lsb_release -is) == opalrtlinux ]; then
      # OPAL-RTLinux CA certificates are outdated
      curl -k  https://curl.se/ca/cacert.pem > /etc/ssl/certs/ca-certificates.crt

      # Broken /var/tmp directory in Docker containers?
      if [ -f /.dockerenv ]; then
            rm -rf /var/tmp
            mkdir -p /var/tmp
      fi

      # Install RT-LAB if installer is available
      RTLAB_PACKAGE="/rtlab/target/rt_linux64/*.rpm"
      if [ -f ${RTLAB_PACKAGE} ]; then
            rpm -i ${RTLAB_PACKAGE}
      fi
fi

rpmbuild -bb \
      --define "srcdir ${SOURCE_DIR}" \
      --verbose \
      --nodebuginfo \
      "${SOURCE_DIR}/packaging/rpm/villas-node.spec"

# Copy RPM back from container back to to host
if [ -f /.dockerenv ]; then
      cp ~/rpmbuild/RPMS/intel_corei7_64/*.rpm ${SOURCE_DIR}
fi
