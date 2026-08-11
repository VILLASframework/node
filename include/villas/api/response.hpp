/* API response.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_map>

#include <villas/api.hpp>
#include <villas/exceptions.hpp>
#include <villas/json.hpp>

namespace villas {
namespace node {
namespace api {

// Forward declarations
class Session;
class Request;

class Response {
public:
  int code = HTTP_STATUS_OK;
  std::string content_type = "text/html; charset=UTF-8";
  std::string body = "";
  std::unordered_map<std::string, std::string> headers = {
      {"Server:", HTTP_USER_AGENT},
      {"Access-Control-Allow-Origin:", "*"},
      {"Access-Control-Allow-Methods:", "GET, POST, OPTIONS"},
      {"Access-Control-Allow-Headers:", "Content-Type"},
      {"Access-Control-Max-Age:", "86400"}};

  static Response json(int, json_t const *);
  static Response json(int, Json const &);
  static Response error(RuntimeError const &);
  static Response error(Error const &);

  int writeBody(struct lws *wsi);
  int writeHeaders(struct lws *wsi);
};

} // namespace api
} // namespace node
} // namespace villas
