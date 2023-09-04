/* Universal Data-exchange API request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/universal.hpp>

using namespace villas::node;
using namespace villas::node::api;
using namespace villas::node::api::universal;

void UniversalRequest::prepare()
{
	NodeRequest::prepare();

	api_node = dynamic_cast<APINode*>(node);
	if (!api_node)
		throw BadRequest("Node {} is not an univeral API node!", node->getNameShort());
}
