/* The API ressource for start/stop/pause/resume paths.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/api.hpp>
#include <villas/api/requests/path.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/path.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

template <auto func> class PathActionRequest : public PathRequest {

public:
  using PathRequest::PathRequest;

  virtual Response *execute() {
    if (method != Session::Method::POST)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("Path endpoints do not accept any body data");

    (path->*func)();

    return new Response(session, HTTP_STATUS_OK);
  }
};

// Register API requests
static char n1[] = "path/start";
static char r1[] = "/path/(" RE_UUID ")/start";
static char d1[] = "start a path";
static RequestPlugin<PathActionRequest<&Path::start>, n1, r1, d1> p1;

static char n2[] = "path/stop";
static char r2[] = "/path/(" RE_UUID ")/stop";
static char d2[] = "stop a path";
static RequestPlugin<PathActionRequest<&Path::stop>, n2, r2, d2> p2;

} // namespace api
} // namespace node
} // namespace villas
