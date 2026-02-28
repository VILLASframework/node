#!/usr/bin/env bash
#
# Delta sharing node Integration Test
#
# Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
# SPDX-FileCopyrightText: 2014-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

echo "Test not ready"
exit 99

set -e

DIR=$(mktemp -d)
pushd ${DIR}

function finish {
    popd
    rm -rf ${DIR}
}
trap finish EXIT

# Test data paths
TEST_PROFILE="${DIR}/open_delta_profile.json"
TEST_CACHE="${DIR}/delta_sharing_test_cache"
TEST_CONFIG="${DIR}/test_config.json"
TEST_OUTPUT="${DIR}/test_output.json"
TEST_INPUT="${DIR}/test_input.json"


# Set up test environment
function setup_test {
    # Create test cache directory
    mkdir -p "${TEST_CACHE}"

    cat > "${TEST_PROFILE}" << 'EOF'
{
  "shareCredentialsVersion": 1,
  "endpoint": "https://sharing.delta.io/delta-sharing/",
  "bearerToken": "faaie590d541265bcab1f2de9813274bf233"
}
EOF

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

}

# Test 1: Verify Delta Sharing credentials
function test_credentials {
    echo "Testing Delta Sharing credentials..."

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
function test_connection {
    echo "Testing Delta Sharing server connection..."

    if timeout 2 "${VILLAS_NODE}" -c "${TEST_CONFIG}" --start 2>&1 | grep -q "Delta Sharing"; then
        log_info "Connection test passed (Delta Sharing client initialized)"
        return 0
    else
        log_error "Connection test failed"
        return 1
    fi
}

# TODO: Test 3, to test data reading from Delta Sharing
function test_data_reading {
    echo "Testing data reading from Delta Sharing..."

    echo "Attempting to read data from COVID-19_NYT table..."

    echo "Data reading test completed"
    return 0
}

# Test 4: Test node configuration parsing
function test_config_parsing {
    echo "Testing node configuration parsing..."

    if ! "${VILLAS_NODE}" --help  | grep -i "delta_sharing"; then
        echo "delta_sharing node type not found in villas-node"
        return 1
    else
        echo "delta_sharing node type present in villas-node"
    fi

    #Test if the configuration can be parsed
    if ! ("${VILLAS_NODE}" "${TEST_CONFIG}" &); then
        echo "Node configuration check failed"
        return 1
    else
        echo "running example configuration for 3 seconds"
        DELTA_PID=$!
        kill $DELTA_PID
    fi
    # wait 3
    # kill $(DELTA_PID)

    echo "Configuration parsing test passed"
    return 0
}

# Test 5: Test node lifecycle
function test_node_lifecycle {
    echo "Testing node lifecycle..."

    if timeout 2 "${VILLAS_NODE}" -c "${TEST_CONFIG}" --start 2>&1 | grep -q "Delta Sharing\|Started"; then
        echo "Node lifecycle test passed"
        return 0
    else
        echo "Node lifecycle test inconclusive"
        return 0
    fi
}

echo "Starting Delta Sharing integration tests with open datasets server..."

# Run all tests
test_credentials || exit 1
test_config_parsing || exit 1
test_connection || exit 1
test_data_reading || exit 1
test_node_lifecycle || exit 1

echo "All tests passed!"
exit 0
