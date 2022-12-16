/** Universal Data-exchange API request.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/nodes/api.hpp>
#include <villas/api/requests/node.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Node;

namespace api {

class UniversalRequest : public NodeRequest {

protected:
	APINode *api_node;

public:
	using NodeRequest::NodeRequest;

	virtual void
	prepare();
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
