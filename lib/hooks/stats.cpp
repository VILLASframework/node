/* Statistic hooks.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <villas/common.hpp>
#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/stats.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class StatsHook;

class StatsWriteHook : public Hook {

protected:
  StatsHook *parent;

public:
  StatsWriteHook(StatsHook *pa, Path *p, Node *n, int fl, int prio,
                 bool en = true)
      : Hook(p, n, fl, prio, en), parent(pa) {
    // This hook has no config. We never call parse() for it
    state = State::PARSED;
  }

  Hook::Reason process(struct Sample *smp) override;
};

class StatsReadHook : public Hook {

protected:
  struct Sample *last;

  StatsHook *parent;

public:
  StatsReadHook(StatsHook *pa, Path *p, Node *n, int fl, int prio,
                bool en = true)
      : Hook(p, n, fl, prio, en), last(nullptr), parent(pa) {
    // This hook has no config. We never call parse() for it
    state = State::PARSED;
  }

  void start() override {
    assert(state == State::PREPARED);

    last = nullptr;

    state = State::STARTED;
  }

  void stop() override {
    assert(state == State::STARTED);

    if (last)
      sample_decref(last);

    state = State::STOPPED;
  }

  Hook::Reason process(struct Sample *smp) override;
};

class StatsHook : public Hook {

  friend StatsReadHook;
  friend StatsWriteHook;

protected:
  std::shared_ptr<StatsReadHook> readHook;
  std::shared_ptr<StatsWriteHook> writeHook;

  enum Stats::Format format;
  int verbose;
  int warmup;
  int buckets;

  std::shared_ptr<Stats> stats;

  FILE *output;
  std::string uri;

public:
  StatsHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), format(Stats::Format::HUMAN), verbose(0),
        warmup(500), buckets(20), output(nullptr), uri() {
    readHook = std::make_shared<StatsReadHook>(this, p, n, fl, prio, en);
    writeHook = std::make_shared<StatsWriteHook>(this, p, n, fl, prio, en);

    if (!readHook || !writeHook)
      throw MemoryAllocationError();

    // Add child hooks
    if (node) {
      node->in.hooks.push_back(readHook);
      node->out.hooks.push_back(writeHook);
    }
  }

  StatsHook &operator=(const StatsHook &) = delete;
  StatsHook(const StatsHook &) = delete;

  void start() override {
    assert(state == State::PREPARED);

    if (!uri.empty()) {
      output = fopen(uri.c_str(), "w+");
      if (!output)
        throw RuntimeError("Failed to open file '{}' for writing", uri);
    }

    state = State::STARTED;
  }

  void stop() override {
    assert(state == State::STARTED);

    stats->print(uri.empty() ? stdout : output, format, verbose);

    if (!uri.empty())
      fclose(output);

    state = State::STOPPED;
  }

  void restart() override {
    assert(state == State::STARTED);

    stats->reset();
  }

  Hook::Reason process(struct Sample *smp) override {
    // Only call readHook if it hasnt been added to the node's hook list
    if (!node)
      return readHook->process(smp);

    return Hook::Reason::OK;
  }

  void periodic() override {
    assert(state == State::STARTED);

    stats->printPeriodic(uri.empty() ? stdout : output, format, node);
  }

  void parse(json_t *json) override {
    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    Hook::parse(json);

    const char *f = nullptr;
    const char *u = nullptr;

    ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: s }",
                         "format", &f, "verbose", &verbose, "warmup", &warmup,
                         "buckets", &buckets, "output", &u);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-stats");

    if (f) {
      try {
        format = Stats::lookupFormat(f);
      } catch (const std::invalid_argument &e) {
        throw ConfigError(json, "node-config-hook-stats",
                          "Invalid statistic output format: {}", f);
      }
    }

    if (u)
      uri = u;

    state = State::PARSED;
  }

  void prepare() override {
    assert(state == State::CHECKED);

    stats = std::make_shared<villas::Stats>(buckets, warmup);

    if (node)
      node->setStats(stats);

    state = State::PREPARED;
  }
};

Hook::Reason StatsWriteHook::process(struct Sample *smp) {
  timespec now = time_now();

  parent->stats->update(Stats::Metric::AGE,
                        time_delta(&smp->ts.received, &now));

  return Reason::OK;
}

Hook::Reason StatsReadHook::process(struct Sample *smp) {
  if (last) {
    if (smp->flags & last->flags & (int)SampleFlags::HAS_TS_RECEIVED)
      parent->stats->update(Stats::Metric::GAP_RECEIVED,
                            time_delta(&last->ts.received, &smp->ts.received));

    if (smp->flags & last->flags & (int)SampleFlags::HAS_TS_ORIGIN)
      parent->stats->update(Stats::Metric::GAP_SAMPLE,
                            time_delta(&last->ts.origin, &smp->ts.origin));

    if ((smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) &&
        (smp->flags & (int)SampleFlags::HAS_TS_RECEIVED))
      parent->stats->update(Stats::Metric::OWD,
                            time_delta(&smp->ts.origin, &smp->ts.received));

    if (smp->flags & last->flags & (int)SampleFlags::HAS_SEQUENCE) {
      int dist = smp->sequence - (int32_t)last->sequence;
      if (dist != 1)
        parent->stats->update(Stats::Metric::SMPS_REORDERED, dist);
    }
  }

  parent->stats->update(Stats::Metric::SIGNAL_COUNT, smp->length);

  sample_incref(smp);

  if (last)
    sample_decref(last);

  last = smp;

  return Reason::OK;
}

// Register hook
static char n[] = "stats";
static char d[] = "Collect statistics for the current node";
static HookPlugin<StatsHook, n, d, (int)Hook::Flags::NODE_READ> p;

} // namespace node
} // namespace villas
