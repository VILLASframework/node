/* REST-API-releated functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>
#include <libwebsockets.h>

#include <atomic>
#include <list>
#include <thread>

#include <libwebsockets.h>

#include <villas/common.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/queue_signalled.hpp>

namespace villas {
namespace node {
namespace api {

const int version = 2;

// Forward declarations
class Session;
class Response;
class Request;

class Error : public RuntimeError {

public:
  template <typename... Args>
  Error(int c = HTTP_STATUS_INTERNAL_SERVER_ERROR,
        const std::string &msg = "Invalid API request", Args &&...args)
      : RuntimeError(msg, std::forward<Args>(args)...), code(c), json(nullptr) {
  }

  template <typename... Args>
  Error(int c = HTTP_STATUS_INTERNAL_SERVER_ERROR,
        const std::string &msg = "Invalid API request",
        const char *fmt = nullptr, Args &&...args)
      : RuntimeError(msg), code(c),
        json(fmt ? json_pack(fmt, std::forward<Args>(args)...) : nullptr) {}

  int code;
  json_t *json;
};

class BadRequest : public Error {

public:
  template <typename... Args>
  BadRequest(const std::string &msg = "Bad API request", Args &&...args)
      : Error(HTTP_STATUS_BAD_REQUEST, msg, std::forward<Args>(args)...) {}
};

class InvalidMethod : public BadRequest {

public:
  InvalidMethod(Request *req);
};

} // namespace api

// Forward declarations
class SuperNode;

class Api {

protected:
  Logger logger;

  enum State state;

  std::thread thread;
  std::atomic<bool> running; // Atomic flag for signalizing thread termination.

  SuperNode *super_node;

  void run();
  void worker();

public:
  /* Initialize the API.
	 *
	 * Save references to list of paths / nodes for command execution.
	 */
  Api(SuperNode *sn);
  ~Api();

  void start();
  void stop();

  SuperNode *getSuperNode() { return super_node; }

  std::list<api::Session *> sessions; // List of currently active connections
  villas::QueueSignalled<api::Session *>
      pending; // A queue of api_sessions which have pending requests.
};

} // namespace node
} // namespace villas
