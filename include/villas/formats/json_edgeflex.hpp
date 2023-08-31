/* JSON serializtion for edgeFlex project.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

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
