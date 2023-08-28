/** UltraLight format for FISMEP project.
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/format.hpp>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class IotAgentUltraLightFormat : public Format {

protected:
	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);

	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);

public:
	using Format::Format;
};

} // namespace node
} // namespace villas
