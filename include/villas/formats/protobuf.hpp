/* Protobuf IO format
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>

#include <villas/format.hpp>

// Generated message descriptors by protoc
#include <villas.pb-c.h>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ProtobufFormat : public BinaryFormat {

protected:
	enum SignalType detect(const Villas__Node__Value *val);

public:
	using BinaryFormat::BinaryFormat;

	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);
	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);
};

} // namespace node
} // namespace villas
