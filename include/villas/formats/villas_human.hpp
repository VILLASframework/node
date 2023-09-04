/* The VILLASframework sample format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdio>

#include <villas/formats/line.hpp>

namespace villas {
namespace node {

class VILLASHumanFormat : public LineFormat {

protected:
	virtual
	size_t sprintLine(char *buf, size_t len, const struct Sample *smp);
	virtual
	size_t sscanLine(const char *buf, size_t len, struct Sample *smp);

public:
	using LineFormat::LineFormat;

	virtual
	void header(FILE *f, const SignalList::Ptr sigs);
};

} // namespace node
} // namespace villas
