/* The Universal Data-exchange API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class ChannelRequest : public UniversalRequest {
public:
  using UniversalRequest::UniversalRequest;

  Response *executeGet(const std::string &signalName, PayloadType payload) {
    if (body != nullptr)
      throw BadRequest("This endpoint does not accept any body data");

    pthread_mutex_lock(&api_node->write.mutex);

    auto *smp = api_node->write.sample;
    if (!smp) {
      pthread_mutex_unlock(&api_node->write.mutex);
      throw Error(HTTP_STATUS_NOT_FOUND, "No data available");
    }

    auto idx = smp->signals->getIndexByName(signalName);
    if (idx < 0) {
      pthread_mutex_unlock(&api_node->write.mutex);
      throw Error(HTTP_STATUS_NOT_FOUND, "Unknown signal id: {}", signalName);
    }

    auto sig = smp->signals->getByIndex(idx);
    auto ch = api_node->write.channels.at(idx);

    if (payload != ch->payload)
      throw BadRequest("Mismatching payload type");

    auto *json_signal =
        json_pack("{ s: f, s: o, s: s, s: s, s: s }", "timestamp",
                  time_to_double(&smp->ts.origin), "value",
                  smp->data[idx].toJson(sig->type), "validity", "unknown",
                  "source", "unknown", "timesource", "unknown");

    if (smp->length <= (unsigned)idx)
      smp->length = idx + 1;

    smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN | (int)SampleFlags::HAS_DATA;

    pthread_mutex_unlock(&api_node->write.mutex);

    return new JsonResponse(session, HTTP_STATUS_OK, json_signal);
  }

  Response *executePut(const std::string &signalName, PayloadType payload) {
    int ret;

    pthread_mutex_lock(&api_node->read.mutex);

    auto *smp = api_node->read.sample;
    if (!smp) {
      pthread_mutex_unlock(&api_node->read.mutex);
      throw Error(HTTP_STATUS_INTERNAL_SERVER_ERROR, "Not initialized yet");
    }

    auto idx = smp->signals->getIndexByName(signalName);
    if (idx < 0) {
      pthread_mutex_unlock(&api_node->read.mutex);
      throw BadRequest("Unknown signal id: {}", signalName);
    }

    auto sig = smp->signals->getByIndex(idx);
    auto ch = api_node->read.channels.at(idx);

    if (payload != ch->payload)
      throw BadRequest("Mismatching payload type");

    double timestamp = 0;
    json_t *json_value;
    const char *validity = nullptr;
    const char *source = nullptr;
    const char *timesource = nullptr;

    json_error_t err;
    ret = json_unpack_ex(body, &err, 0, "{ s: F, s: o, s?: s, s?: s, s?: s }",
                         "timestamp", &timestamp, "value", &json_value,
                         "validity", &validity, "timesource", &timesource,
                         "source", &source);
    if (ret) {
      pthread_mutex_unlock(&api_node->read.mutex);
      throw BadRequest("Malformed body: {}", err.text);
    }

    if (validity)
      logger->warn("Attribute 'validity' is not supported by VILLASnode");

    if (source)
      logger->warn("Attribute 'source' is not supported by VILLASnode");

    if (timesource)
      logger->warn("Attribute 'timesource' is not supported by VILLASnode");

    ret = smp->data[idx].parseJson(sig->type, json_value);
    if (ret) {
      pthread_mutex_unlock(&api_node->read.mutex);
      throw BadRequest("Malformed value");
    }

    smp->ts.origin = time_from_double(timestamp);

    pthread_cond_signal(&api_node->read.cv);
    pthread_mutex_unlock(&api_node->read.mutex);

    return new JsonResponse(session, HTTP_STATUS_OK, json_object());
  }

  Response *execute() override {
    auto const &signalName = matches[2];
    auto const &subResource = matches[3];

    PayloadType payload;
    if (subResource == "event")
      payload = PayloadType::EVENTS;
    else if (subResource == "sample")
      payload = PayloadType::EVENTS;
    else
      throw BadRequest("Unsupported sub-resource: {}", subResource);

    switch (method) {
    case Session::Method::GET:
      return executeGet(signalName, payload);
    case Session::Method::PUT:
      return executePut(signalName, payload);
    default:
      throw InvalidMethod(this);
    }
  }
};

// Register API requests
static char n[] = "universal/channel/sample";
static char r[] =
    "/universal/(" RE_NODE_NAME ")/channel/([a-z0-9_-]+)/(sample|event)";
static char d[] = "Retrieve or send samples via universal data-exchange API";
static RequestPlugin<ChannelRequest, n, r, d> p;

} // namespace universal
} // namespace api
} // namespace node
} // namespace villas
