/* API Response.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/config.hpp>
#include <villas/jansson.hpp>

using namespace villas::node::api;

Response Response::json(int code, json_t const *json) {
  std::string body;

  auto callback = [](const char *data, size_t len, void *ctx) {
    auto &string = *reinterpret_cast<std::string *>(ctx);
    string.insert(string.end(), data, data + len);
    return 0;
  };

  json_dump_callback(json, callback, &body, JSON_INDENT(4));
  return Response(code, "application/json", body);
}

Response Response::json(int code, Json const &json) {
  return Response(code, "application/json", json.dump(4));
}

Response Response::error(RuntimeError const &err) {
  return Response::json(HTTP_STATUS_INTERNAL_SERVER_ERROR,
                        Json::object({{"error", err.what()}}));
}

Response Response::error(Error const &err) {
  auto response = JanssonPtr(json_pack("{ s: s }", "error", err.what()));
  if (err.json)
    json_object_update(response.get(), err.json);
  return Response::json(err.code, response.get());
}

int Response::writeBody(struct lws *wsi) {
  int ret;

  ret = lws_write(wsi, (unsigned char *)body.data(), body.size(),
                  LWS_WRITE_HTTP_FINAL);
  if (ret < 0)
    return -1;

  return 1;
}

int Response::writeHeaders(struct lws *wsi) {
  int ret;
  uint8_t headerBuffer[2048], *p = headerBuffer,
                              *end = &headerBuffer[sizeof(headerBuffer) - 1];

  ret = lws_add_http_common_headers(wsi, code, content_type.c_str(),
                                    body.size(), &p, end);
  if (ret)
    return 1;

  for (auto &hdr : headers) {
    ret = lws_add_http_header_by_name(
        wsi, reinterpret_cast<const unsigned char *>(hdr.first.c_str()),
        reinterpret_cast<const unsigned char *>(hdr.second.c_str()),
        hdr.second.size(), &p, end);
    if (ret)
      return -1;
  }

  ret = lws_finalize_write_http_header(wsi, headerBuffer, &p, end);
  if (ret)
    return 1;

  // Do we have a body to send?
  if (body.size() > 0)
    lws_callback_on_writable(wsi);

  return 0;
}
