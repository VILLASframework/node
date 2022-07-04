/** The API ressource for start/stop/pause/resume nodes.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <jansson.h>

#include <villas/node.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/api/requests/node.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

template<auto func>
class NodeActionRequest : public NodeRequest  {

public:
	using NodeRequest::NodeRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Node endpoints do not accept any body data");

		int ret = (node->*func)();
		if (ret)
			throw BadRequest("Failed to execute action", "{ s: d }",
				"ret", ret
			);

		return new Response(session, HTTP_STATUS_OK);
	}

};

/* Register API requests */
static char n1[] = "node/start";
static char r1[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/start";
static char d1[] = "start a node";
static RequestPlugin<NodeActionRequest<&Node::start>, n1, r1, d1> p1;

static char n2[] = "node/stop";
static char r2[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/stop";
static char d2[] = "stop a node";
static RequestPlugin<NodeActionRequest<&Node::stop>, n2, r2, d2> p2;

static char n3[] = "node/pause";
static char r3[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/pause";
static char d3[] = "pause a node";
static RequestPlugin<NodeActionRequest<&Node::pause>, n3, r3, d3> p3;

static char n4[] = "node/resume";
static char r4[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/resume";
static char d4[] = "resume a node";
static RequestPlugin<NodeActionRequest<&Node::resume>, n4, r4, d4> p4;

static char n5[] = "node/restart";
static char r5[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/restart";
static char d5[] = "restart a node";
static RequestPlugin<NodeActionRequest<&Node::restart>, n5, r5, d5> p5;


} /* namespace api */
} /* namespace node */
} /* namespace villas */
