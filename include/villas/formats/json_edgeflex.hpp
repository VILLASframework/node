/** JSON serializtion for edgeFlex project.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <villas/formats/json.hpp>

namespace villas {
namespace node {

class JsonEdgeflexFormat : public JsonFormat {

protected:
	int packSample(json_t **j, const struct sample *smp);
	int unpackSample(json_t *json_smp, struct sample *smp);

public:
	using JsonFormat::JsonFormat;
};

} /* namespace node */
} /* namespace villas */
