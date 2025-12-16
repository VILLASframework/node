/* The "restart" API request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/log.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

class RestartRequest : public Request {

protected:
  static std::string configUri;

  static void handler() {
    int ret;
    const char *cfg = !configUri.empty() ? configUri.c_str() : nullptr;

    const char *argv[] = {"villas-node", cfg, nullptr};

    Logger logger = Log::get("api:restart");

    if (cfg)
      logger->info("Restarting instance: config={}", cfg);
    else
      logger->info("Restarting instance");

    ret = execvp("/proc/self/exe", (char **)argv);
    if (ret)
      throw SystemError("Failed to restart");
  }

public:
  using Request::Request;

  Response *execute() override {
    int ret;
    json_error_t err;

    if (method != Session::Method::POST)
      throw Error::invalidMethod(this);

    json_t *json_config = nullptr;

    if (body) {
      ret = json_unpack_ex(body, &err, 0, "{ s?: o }", "config", &json_config);
      if (ret < 0)
        throw Error::badRequest(nullptr, "Failed to parse request body");
    }

    if (json_config) {
      if (json_is_string(json_config))
        configUri = json_string_value(json_config);
      else if (json_is_object(json_config)) {
        char configUriBuf[] = "villas-node.json.XXXXXX";
        int configFd = mkstemp(configUriBuf);

        FILE *configFile = fdopen(configFd, "w+");

        ret = json_dumpf(json_config, configFile, JSON_INDENT(4));
        if (ret < 0)
          throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, nullptr,
                      "Failed to create temporary config file");

        fclose(configFile);
        configUri = configUriBuf;
      } else if (json_config != nullptr)
        throw Error::badRequest(
            nullptr, "Parameter 'config' must be either a URL (string) or "
                     "a configuration (object)");
    } else // If no config is provided via request, we will use the previous one
      configUri = session->getSuperNode()->getConfigPath();

    logger->info("Restarting to {}", configUri);

    // Increment API restart counter
    char *scnt = getenv("VILLAS_API_RESTART_COUNT");
    int cnt = scnt ? atoi(scnt) : 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", cnt + 1);

    // We pass some env variables to the new process
    setenv("VILLAS_API_RESTART_COUNT", buf, 1);

    auto *json_response = json_pack(
        "{ s: i, s: o }", "restarts", cnt, "config",
        configUri.empty() ? json_null() : json_string(configUri.c_str()));

    // Register exit handler
    ret = atexit(handler);
    if (ret)
      throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, nullptr,
                  "Failed to restart VILLASnode instance");

    // Properly terminate current instance
    utils::killme(SIGTERM);

    return new JsonResponse(session, HTTP_STATUS_OK, json_response);
  }
};

std::string RestartRequest::configUri;

// Register API request
static char n[] = "restart";
static char r[] = "/restart";
static char d[] = "Restart VILLASnode with new configuration";
static RequestPlugin<RestartRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
