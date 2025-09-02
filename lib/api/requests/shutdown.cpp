/* The "shutdown" API request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <signal.h>

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

class ShutdownRequest : public Request {

public:
  using Request::Request;

  Response *execute() override {
    if (method != Session::Method::POST)
      throw Error::invalidMethod(this);

    if (body != nullptr)
      throw Error::badRequest(
          nullptr, "Shutdown endpoint does not accept any body data");

    utils::killme(SIGTERM);

    return new Response(session, HTTP_STATUS_OK);
  }
};

// Register API request
static char n[] = "shutdown";
static char r[] = "/shutdown";
static char d[] = "Terminate VILLASnode instance";
static RequestPlugin<ShutdownRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
