/* Node type: API gateway.
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <pthread.h>

#include <villas/exceptions.hpp>
#include <villas/nodes/gateway.hpp>

#include "villas/sample.hpp"

using namespace villas;
using namespace villas::node;

GatewayNode::GatewayNode(const uuid_t &id, const std::string &name)
    : Node(id, name), read(), write() {
  int ret;
  auto dirs = std::vector{&read, &write};

  for (auto dir : dirs) {
    ret = pthread_mutex_init(&dir->mutex, nullptr);
    if (ret)
      throw RuntimeError("failed to initialize mutex");

    ret = pthread_cond_init(&dir->cv, nullptr);
    if (ret)
      throw RuntimeError("failed to initialize mutex");
  }
}

int GatewayNode::prepare() {

  read.sample = sample_alloc_mem(64);
  if (!read.sample)
    throw MemoryAllocationError();

  write.sample = sample_alloc_mem(64);
  if (!write.sample)
    throw MemoryAllocationError();

  return Node::prepare();
}

int GatewayNode::check() { return Node::check(); }

int GatewayNode::_read(struct Sample *smps[], unsigned cnt) {
  assert(cnt == 1);

  pthread_cond_wait(&read.cv, &read.mutex);
  sample_copy(smps[0], read.sample);

  return 1;
}

int GatewayNode::_write(struct Sample *smps[], unsigned cnt) {
  assert(cnt == 1);
  sample_copy(write.sample, smps[0]);

  int ret =
      formatter->sprint(write.buf, write.buflen, &write.wbytes, smps, cnt);
  if (ret < 0) {
    logger->warn("Failed to format payload: reason={}", ret);
    return ret;
  }

  pthread_cond_signal(&write.cv);

  return 1;
}

int GatewayNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  json_t *json_format = nullptr;
  // json_t *remote = nullptr;
  const char *endpoint_address, *gateway_type;
  json_error_t err;
  ret = json_unpack_ex(json, &err, 0, "{ s?:o, s:s, s:s}", "format",
                       &json_format, "gateway_type", &gateway_type, "address",
                       &endpoint_address);
  if (ret)
    throw ConfigError(json, err, "node-config-node-gateway");

  formatter = json_format ? FormatFactory::make(json_format)
                          : FormatFactory::make("villas.binary");
  if (!formatter)
    throw ConfigError(json_format, "node-config-node-gateway-format",
                      "Invalid format configuration");

  address = endpoint_address;
  if (!strcmp(gateway_type, "gRPC")) {
    type = ApiType::gRPC;
  } else {
    throw SystemError("Invalid api type: {}", gateway_type);
  }

  return 0;
}

int GatewayNode::start() {
  formatter->start(getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  read.buflen = 64 * 1024;
  read.buf = new char[read.buflen];
  write.buflen = 64 * 1024;
  write.buf = new char[write.buflen];

  return Node::start();
}

int GatewayNode::stop() {
  delete[] read.buf;
  delete[] write.buf;
  sample_free(read.sample);
  sample_free(write.sample);
  return 0;
}

GatewayNode::~GatewayNode() {}

// Register node
static char n[] = "gateway";
static char d[] = "A node providing a Gateway";
static NodePlugin<GatewayNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE,
                  1>
    p;
