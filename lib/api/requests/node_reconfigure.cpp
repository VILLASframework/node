/* The API ressource for reconfiguring a node.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <set>

#include <libwebsockets.h>

#include <villas/api.hpp>
#include <villas/api/requests/node.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/jansson.hpp>
#include <villas/json.hpp>
#include <villas/node.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

#include "villas/exceptions.hpp"

namespace villas {
namespace node {
namespace api {

class NodeReconfigureRequest : public NodeRequest {

public:
  using NodeRequest::NodeRequest;

  Response execute() override {
    auto &config = node->getConfig();

    if (method == Session::Method::GET) {
      if (body != nullptr)
        throw Error::badRequest(nullptr, "Found non-empty body on GET request");

      auto reconfigurable = Json::object();
      for (auto const &[ptr, callback] : node->reconfiguration_callbacks)
        reconfigurable[ptr.to_string()] =
            config.contains(ptr) ? config.at(ptr) : Json();

      return Response::json(HTTP_STATUS_OK, reconfigurable);
    } else if (method != Session::Method::POST) {
      throw Error::invalidMethod(this);
    }

    if (not node->isEnabled())
      throw Error::badRequest(nullptr, "This node is disabled");

    if (body == nullptr)
      throw Error::badRequest(nullptr, "Missing body on POST request");

    // Strip any parameters (e.g. '; charset=utf-8') from the media type.
    auto content_type = getHeader(WSI_TOKEN_HTTP_CONTENT_TYPE);
    content_type = content_type.substr(0, content_type.find(';'));

    Json requested_config;
    if (content_type == "application/json-patch+json") {
      try {
        requested_config = config.patch(body);
      } catch (Json::exception const &error) {
        throw Error::badRequest(nullptr, "Failed to apply JSON patch: {}",
                                error.what());
      }
    } else if (content_type == "application/merge-patch+json") {
      requested_config = config;
      requested_config.merge_patch(body);
    } else if (content_type == "application/json") {
      requested_config = body;
    } else {
      throw Error(HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE, nullptr,
                  "Unsupported content type: {}", content_type);
    }

    try {
      node->getFactory()->getSchema().validate(requested_config);
    } catch (JsonError &error) {
      auto diagnostics = Json::object();
      for (auto const &diagnostic : error)
        diagnostics[diagnostic.pointer.to_string()] = diagnostic.message;

      auto json_error = Json::object({{"diagnostics", std::move(diagnostics)}});
      throw Error::badRequest(json_error.get<JanssonPtr>().release(),
                              "Invalid node configuration");
    }

    auto requested_changes = std::set<JsonPointer>();
    auto unsupported = Json::array();
    for (auto const &op : Json::diff(config, requested_config)) {
      auto path = JsonPointer(op.at("path"));
      logger->debug("trying to reconfigure {}", path.to_string());

      auto found = false;
      for (auto const &[ptr, callback] : node->reconfiguration_callbacks) {
        auto iter = path;

        do {
          if (iter == ptr) {
            requested_changes.insert(ptr);
            found = true;
            break;
          }

          iter.pop_back();
        } while (not iter.empty());

        if (found)
          break;
      }

      if (not found)
        unsupported.push_back(path);
    }

    if (not unsupported.empty()) {
      auto json_error = Json::object({{"unsupported", std::move(unsupported)}});
      throw Error::badRequest(
          json_error.get<JanssonPtr>().release(),
          "The node does not support reconfiguring these settings at runtime");
    }

    for (auto const &ptr : requested_changes) {
      node->logger->debug("reconfigure[{}]", ptr.to_string());
      auto callback = node->reconfiguration_callbacks.at(ptr);
      auto const &requested = requested_config.at(ptr);
      try {
        callback(requested);
        config[ptr] = requested;
      } catch (std::exception &error) {
        throw RuntimeError("Failed to reconfigure {}: {}", ptr, error.what());
      }
    }

    return Response(HTTP_STATUS_OK);
  }
};

// Register API request
static char n[] = "node/reconfigure";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/reconfigure";
static char d[] = "Reconfigure a node at runtime";
static RequestPlugin<NodeReconfigureRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
