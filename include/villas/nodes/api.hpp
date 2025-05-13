/* Node type: Universal Data-exchange API (v2).
 *
 * @see https://github.com/ERIGrid2/JRA-3.1-api
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <pthread.h>

#include <villas/api/universal.hpp>
#include <villas/node.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class APINode : public Node {

public:
  APINode(const uuid_t &id = {}, const std::string &name = "");

  struct Direction {
    Sample *sample;
    api::universal::ChannelList channels;
    pthread_cond_t cv;
    pthread_mutex_t mutex;
  };

  // Accessed by api::universal::SignalRequest
  Direction read, write;

  int prepare() override;

  int check() override;

protected:
  int parse(json_t *json) override;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;
};

} // namespace node
} // namespace villas
