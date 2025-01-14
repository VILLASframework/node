#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

ARGS="$@"

FPGA=root@137.226.133.220

BUILDDIR=${BUILDDIR:-$(pwd)}

mkdir -p out
make install DESTDIR=${BUILDDIR}/out

ssh-copy-id -i ~/.ssh/id_rsa ${FPGA}

rsync --progress -a ${BUILDDIR}/out/usr/local/ ${FPGA}:/villas/

ssh ${FPGA} <<'ENDSSH'
# commands to run on remote host

mkdir -p /etc/ld.so.conf.d/
echo "/villas/lib" > /etc/ld.so.conf.d/villas.conf
ldconfig
villas-node -V
ENDSSH

ssh ${FPGA} /villas/bin/villas-node
