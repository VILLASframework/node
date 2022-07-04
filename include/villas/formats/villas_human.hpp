/** The VILLASframework sample format
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

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

} /* namespace node */
} /* namespace villas */
