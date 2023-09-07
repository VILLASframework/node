/* The "file" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/node.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/super_node.hpp>

#include <villas/format.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/file.hpp>

namespace villas {
namespace node {
namespace api {

class FileRequest : public NodeRequest {

public:
  using NodeRequest::NodeRequest;

  virtual Response *execute() {
    if (method != Session::Method::GET && method != Session::Method::POST)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("File endpoint does not accept any body data");

    NodeFactory *nf = plugin::registry->lookup<NodeFactory>("file");

    if (node->getFactory() != nf)
      throw BadRequest("This node is not a file node", "{ s: s }", "type",
                       node->getFactory()->getName());

    auto *nc = dynamic_cast<NodeCompat *>(node);
    auto *f = nc->getData<struct file>();

    if (matches[2] == "rewind")
      rewind(f->stream_in);

    return new Response(session, HTTP_STATUS_OK);
  }
};

// Register API request
static char n[] = "node/file";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/file(?:/([^/]+))?";
static char d[] = "control instances of 'file' node-type";
static RequestPlugin<FileRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
