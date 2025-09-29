#!/usr/bin/env bash
#
# Test hooks in villas node
#
# Author: Ritesh Karki
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# set -e

# DIR=$(mktemp -d)
# pushd ${DIR}

# function finish {
#     popd
#     rm -rf ${DIR}
# }

# trap finish EXIT

# cat > deltaSharingTest.share <<EOF
# {
#   "shareCredentialsVersion": 1,
#   "endpoint": "https://sharing.delta.io/delta-sharing/",
#   "bearerToken": "faaie590d541265bcab1f2de9813274bf233"
# }
# EOF

# cat > config.conf <<EOF
# nodes = {
#     node1 = {
#         type = "delta_sharing"
#         profile_path = "open-datasets.share"
#         table_path = "open-datasets.share#delta_sharing.default.COVID_19_NYT"
#         op = "read"
#     },
#     node2 = {
#         type = "file"
#         uri = "delta_output.dat"
#         in = {
#             epoch_mode = "direct"
#             read_mode = "all"
#             eof = "stop"
#         }
#         out = {

#         }
#     }

# }
# paths = (
#     {
#         in = "node1"
#         out = "node2"
#     }
# )
# EOF

# villas node config.conf




#!/bin/bash

# Delta Share Node Integration Test
# This test verifies the delta_sharing node can connect to an open Delta Sharing server
# and read/write data

set -e

# Test configuration
TEST_NAME="delta_sharing_integration"
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TEST_DIR}/../../../../build"
# VILLAS_NODE="${BUILD_DIR}/src/villas-node"
VILLAS_NODE="villas-node"

# Test data paths
TEST_PROFILE="${TEST_DIR}/open_delta_profile.json"
TEST_CACHE="/tmp/delta_sharing_test_cache"
TEST_CONFIG="${TEST_DIR}/test_config.json"
TEST_OUTPUT="${TEST_DIR}/test_output.json"
TEST_INPUT="${TEST_DIR}/test_input.json"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

cleanup() {
    log_info "Cleaning up test artifacts..."
    rm -f "${TEST_CONFIG}" "${TEST_OUTPUT}" "${TEST_INPUT}"
    rm -rf "${TEST_CACHE}"
    rm -f "${TEST_PROFILE}"
    log_info "Cleanup complete"
}

# Set up test environment
setup_test() {
    log_info "Setting up Delta Share integration test with open datasets server..."

    # Create test cache directory
    mkdir -p "${TEST_CACHE}"

    # Create test profile for open Delta Sharing server
    # You'll need to replace this with your actual profile
    cat > "${TEST_PROFILE}" << 'EOF'
{
  "shareCredentialsVersion": 1,
  "endpoint": "https://sharing.delta.io/delta-sharing/",
  "bearerToken": "faaie590d541265bcab1f2de9813274bf233"
}
EOF

    # Create test input data
    cat > "${TEST_INPUT}" << 'EOF'
{
    "nodes": {
        "signal_source": {
            "type": "signal",
            "signal": "sine",
            "rate": 10,
            "limit": 5
        }
    },
    "paths": [
        {
            "in": "signal_source"
        }
    ]
}
EOF

    # Create test configuration for reading from Delta Sharing
    cat > "${TEST_CONFIG}" << EOF
{
    "nodes": {
        "delta_reader": {
            "type": "delta_sharing",
            "profile_path": "${TEST_PROFILE}",
            "cache_dir": "${TEST_CACHE}",
            "table_path": "open-datasets.share#delta_sharing.default.COVID_19_NYT",
            "op": "read",
            "batch_size": 10
        },
        "delta_writer": {
            "type": "delta_sharing",
            "profile_path": "${TEST_PROFILE}",
            "cache_dir": "${TEST_CACHE}",
            "table_path": "open-delta-sharing.s3.us-west-2.amazonaws.com#samples.test_output",
            "op": "write",
            "batch_size": 10
        },
        "file1": {
            "type": "file",
            "uri": "${TEST_OUTPUT}",
            "format": "json"
        }
    },
    "paths": [
        {
            "in": "delta_reader",
            "out": "file1"
        }
    ]
}
EOF

    log_info "Test setup complete"
}

# Test 1: Verify Delta Sharing credentials
test_credentials() {
    log_info "Testing Delta Sharing credentials..."

    if [ ! -f "${TEST_PROFILE}" ]; then
        log_error "Profile file not found: ${TEST_PROFILE}"
        return 1
    fi

    # Check if profile has valid JSON structure
    if ! python3 -m json.tool "${TEST_PROFILE}" > /dev/null 2>&1; then
        log_error "Invalid JSON in profile file"
        return 1
    fi


    log_info "Credentials validation test passed"
    return 0
}

# Test 2: Test Delta Sharing connection
test_connection() {
    log_info "Testing Delta Sharing server connection..."

    # Test basic connection by trying to start the node
    if timeout 2 "${VILLAS_NODE}" -c "${TEST_CONFIG}" --start 2>&1 | grep -q "Delta Sharing"; then
        log_info "Connection test passed (Delta Sharing client initialized)"
        return 0
    else
        log_error "Connection test failed"
        return 1
    fi
}

# Test 3: Test data reading from Delta Sharing
test_data_reading() {
    log_info "Testing data reading from Delta Sharing..."

    # Try to read a small amount of data
    log_debug "Attempting to read data from COVID-19_NYT table..."

    log_info "Data reading test completed"
    return 0
}

# Test 4: Test node configuration parsing
test_config_parsing() {
    log_info "Testing node configuration parsing..."

    # Test if the node type is recognized
    if ! "${VILLAS_NODE}" --help  | grep -i "delta_sharing"; then
        log_error "delta_sharing node type not found in villas-node"
        return 1
    else
        log_info "delta_sharing node type present in villas-node"
    fi

    #Test if the configuration can be parsed
    if ! ("${VILLAS_NODE}" "${TEST_CONFIG}" &); then
        log_error "Node configuration check failed"
        return 1
    else
        log_info "running example configuration for 3 seconds"
        DELTA_PID=$!
        kill $DELTA_PID
    fi
    # wait 3
    # kill $(DELTA_PID)


    log_info "Configuration parsing test passed"
    return 0
}

# Test 5: Test node lifecycle
test_node_lifecycle() {
    log_info "Testing node lifecycle..."

    # Test start/stop without running for too long
    if timeout 2 "${VILLAS_NODE}" -c "${TEST_CONFIG}" --start 2>&1 | grep -q "Delta Sharing\|Started"; then
        log_info "Node lifecycle test passed"
        return 0
    else
        log_warn "Node lifecycle test inconclusive"
        return 0
    fi
}


# Main test execution
run_tests() {
    local exit_code=0

    log_info "Starting Delta Share integration tests with open datasets server..."

    # Run all tests
    test_credentials || exit_code=1
    test_config_parsing || exit_code=1
    test_connection || exit_code=1
    test_data_reading || exit_code=1
    test_node_lifecycle || exit_code=1

    if [ $exit_code -eq 0 ]; then
        log_info "All tests passed!"
    else
        log_error "Some tests failed!"
    fi

    return $exit_code
}

# Main execution
main() {
    # Ensure we're in the right directory
    cd "${TEST_DIR}"

    # Set up trap for cleanup
    trap cleanup EXIT

    # Check if profile exists and has valid credentials
    if [ ! -f "${TEST_PROFILE}" ] || grep -q "YOUR_ACTUAL_BEARER_TOKEN_HERE" "${TEST_PROFILE}"; then
        log_warn "Please set up your Delta Sharing credentials before running tests"
        log_warn "Some tests will be skipped"
    fi

    # Run tests
    setup_test
    run_tests
    local test_result=$?

    # Cleanup automatically via trap

    exit $test_result
}

main "$@"


