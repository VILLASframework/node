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
GITHUB_REPOSITORY=${GITHUB_REPOSITORY:-VILLASframework/node}
VILLAS_TAG=${VILLAS_TAG:-latest}

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

RELEASE_ASSET="villas-${ARCH}-${OS}"

if [[ "${VILLAS_TAG}" == "latest" ]]; then
    DOWNLOAD_URL="https://github.com/${GITHUB_REPOSITORY}/releases/latest/download/${RELEASE_ASSET}"
else
    DOWNLOAD_URL="https://github.com/${GITHUB_REPOSITORY}/releases/download/${VILLAS_TAG}/${RELEASE_ASSET}"
fi

echo "=== [info]  Downloading VILLASnode binary"

if command -v curl >/dev/null 2>&1; then
    echo "  from ${DOWNLOAD_URL}"
    echo "  to ${DOWNLOAD_PATH}"

    if ! curl \
        --fail-with-body \
        --location \
        --header "Accept-Encoding: gzip, deflate" \
        --output "${DOWNLOAD_PATH}" \
         "${DOWNLOAD_URL}"; then
        echo "=== [error] curl failed to download VILLASnode binary"
        echo "=== [info]  Tried release tag: ${VILLAS_TAG}"
        echo "=== [info]  Override with: VILLAS_TAG=<tag> ./tools/villas-install.sh"
        exit 1
    fi
else
    echo "=== [error] curl is not available. Please install it."
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
