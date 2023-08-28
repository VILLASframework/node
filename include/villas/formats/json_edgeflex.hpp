/** JSON serializtion for edgeFlex project.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/formats/json.hpp>

namespace villas {
namespace node {

class JsonEdgeflexFormat : public JsonFormat {

protected:
	virtual
	int packSample(json_t **j, const struct Sample *smp);
	virtual
	int unpackSample(json_t *json_smp, struct Sample *smp);

public:
	using JsonFormat::JsonFormat;
};

} // namespace node
} // namespace villas
