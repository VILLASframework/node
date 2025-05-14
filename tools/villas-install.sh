#!/usr/bin/env bash
#
# A small shell script which download a single-binary / standalone build of VILLASnode and installs it
#
# Author: Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-FileCopyrightText: 2025 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

set -e

# Parameters
INSTALL_DIR=${INSTALL_DIR:-/usr/local/bin}
INSTALL_PATH="${INSTALL_DIR}/villas"

echo "==="
echo "=== This script will download a single-binary / standalone build of VILLASnode and install it to ${INSTALL_PATH}"
echo "==="

# Create a temporary directory for the download
TMPDIR=$(mktemp -d)
trap 'rm -rf ${TMPDIR}' EXIT

DOWNLOAD_PATH="${TMPDIR}/villas"

# OS and architecture detection
OS1=$(uname -s)
OS=${OS1,,}
ARCH=$(uname -m)

# Check if the OS is supported
if [[ "${OS}" != "linux" ]]; then
    echo "=== [error] Unsupported OS: ${OS}"
    exit 1
fi

if [[ "${ARCH}" != "x86_64" && "${ARCH}" != "aarch64" ]]; then
    echo "=== [error] Unsupported architecture: ${ARCH}"
    exit 1
fi

echo "=== [info]  Detected supported system: ${ARCH}-${OS}"

GITLAB_URL="https://git.rwth-aachen.de"
GITLAB_PROJECT="79039"
GITLAB_BRANCH="master"
GITLAB_ARTIFACT="artifacts/villas-${ARCH}-${OS}"
GITLAB_JOB="pkg:nix:arx: [${ARCH}-${OS}]"

DOWNLOAD_URL="${GITLAB_URL}/api/v4/projects/${GITLAB_PROJECT}/jobs/artifacts/${GITLAB_BRANCH}/raw/${GITLAB_ARTIFACT}"

echo "=== [info]  Downloading VILLASnode binary"

if $(command -v curl >/dev/null 2>&1); then
    echo "  from ${DOWNLOAD_URL}?job=${GITLAB_JOB}"
    echo "  to ${DOWNLOAD_PATH}"

    if ! curl \
        --fail-with-body \
        --get \
        --location \
        --header "Accept-Encoding: gzip, deflate" \
        --output "${DOWNLOAD_PATH}" \
        --data-urlencode "job=${GITLAB_JOB}" \
         "${DOWNLOAD_URL}"; then
        echo "=== [error] curl failed to download VILLASnode binary"
        exit 1
    fi
else
    echo "=== [error] curl is not available. Please install it."
    exit 1
fi
if [[ $? -ne 0 ]]; then
    echo "=== [error] Failed to download VILLASnode binary from ${DOWNLOAD_URL}"
    exit 1
fi

chmod +x "${DOWNLOAD_PATH}"

if [[ $EUID -ne 0 ]]; then
    echo "=== Warn:  Script started as non-root user."
    echo "=== [info]  Elevating privileges with sudo ..."
    SUDO="sudo"
else
    SUDO=""
fi

if [[ ! -d "${INSTALL_DIR}" ]]; then
    echo "=== [info]  Creating directory ${INSTALL_PATH%/*} ..."
    ${SUDO} mkdir -p "${INSTALL_PATH%/*}"
fi

echo "=== [info]  Installing VILLASnode binary to ${INSTALL_PATH} ..."
${SUDO} mv "${DOWNLOAD_PATH}" "${INSTALL_PATH}"

echo "=== [info]  VILLASnode binary installed successfully to ${INSTALL_PATH}"
echo "=== [info]  Installed VILLASnode version:"
${INSTALL_PATH} node -V
