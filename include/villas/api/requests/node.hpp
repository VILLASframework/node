/** API Request for nodes.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <uuid/uuid.h>

#include <villas/api/request.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Node;

namespace api {

class NodeRequest : public Request {

protected:
	Node *node;

public:
	using Request::Request;

	virtual void
	prepare();
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
