/* API Request for paths.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/request.hpp>

namespace villas {
namespace node {

// Forward declarations
class Path;

namespace api {

class PathRequest : public Request {

protected:
  class Path *path;

public:
  using Request::Request;

  void prepare() override;
};

} // namespace api
} // namespace node
} // namespace villas
