/* Redis node-type.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <thread>
#include <unordered_map>

#include <sw/redis++/redis++.h>

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/pool.hpp>
#include <villas/queue_signalled.h>
#include <villas/task.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

enum class RedisMode { KEY, HASH, CHANNEL };

class RedisConnection {

public:
  sw::redis::Redis context;

protected:
  enum State { INITIALIZED, RUNNING, STOPPING };

  std::thread thread;
  std::atomic<enum State> state;

  void onMessage(const std::string &channel, const std::string &msg);

  void loop();

  std::unordered_multimap<std::string, NodeCompat *> subscriberMap;

  sw::redis::Subscriber subscriber;

  villas::Logger logger;

public:
  RedisConnection(const sw::redis::ConnectionOptions &opts);

  static RedisConnection *get(const sw::redis::ConnectionOptions &opts);

  void start();
  void stop();

  void subscribe(NodeCompat *n, const std::string &channel);
  void unsubscribe(NodeCompat *n, const std::string &channel);
};

struct redis {
  sw::redis::ConnectionOptions options;

  RedisConnection *conn;

  enum RedisMode mode;

  std::string key;

  bool notify; // Use Redis Keyspace notifications to listen for updates.

  struct Task task; // Timer for periodic events.
  double
      rate; // Rate for polling key updates if keyspace notifications are disabled.

  Format *formatter;

  struct Pool pool;
  struct CQueueSignalled queue;
};

int redis_init(NodeCompat *n);

int redis_destroy(NodeCompat *n);

int redis_parse(NodeCompat *n, json_t *json);

char *redis_print(NodeCompat *n);

int redis_check(NodeCompat *n);

int redis_prepare(NodeCompat *n);

int redis_start(NodeCompat *n);

int redis_stop(NodeCompat *n);

int redis_pause(NodeCompat *n);

int redis_resume(NodeCompat *n);

int redis_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int redis_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int redis_reverse(NodeCompat *n);

int redis_poll_fds(NodeCompat *n, int fds[]);

int redis_netem_fds(NodeCompat *n, int fds[]);

} // namespace node
} // namespace villas
