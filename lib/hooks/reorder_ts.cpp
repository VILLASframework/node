/* Reorder samples hook.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2023, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <vector>

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class ReorderTsHook : public Hook {

protected:
  std::vector<Sample *> window;
  std::size_t window_size;
  Sample *buffer;

  void swapSample(Sample *lhs, Sample *rhs) {
    if (buffer) {
      sample_copy(buffer, lhs);
      sample_copy(lhs, rhs);
      sample_copy(rhs, buffer);
    } else {
      buffer = sample_clone(lhs);
      sample_copy(lhs, rhs);
      sample_copy(rhs, buffer);
    }
  }

public:
  ReorderTsHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), window{}, window_size(16), buffer(nullptr) {}

  virtual void parse(json_t *json) {
    assert(state != State::STARTED);

    json_error_t err;
    int ret =
        json_unpack_ex(json, &err, 0, "{ s?: i }", "window_size", &window_size);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-reorder-ts");

    state = State::PARSED;
  }

  virtual void start() {
    assert(state == State::PREPARED || state == State::STOPPED);

    window.reserve(window_size);

    state = State::STARTED;
  }

  virtual void stop() {
    assert(state == State::STARTED);

    for (auto sample : window)
      sample_free(sample);

    if (buffer)
      sample_free(buffer);

    window.clear();

    state = State::STOPPED;
  }

  virtual Hook::Reason process(Sample *smp) {
    assert(state == State::STARTED);
    assert(smp);

    if (window.empty()) {
      window.push_back(sample_clone(smp));

      logger->debug("window.size={}/{}", window.size(), window_size);

      return Hook::Reason::SKIP_SAMPLE;
    }

    for (std::size_t i = window.size() - 1;; i--) {
      if (time_cmp(&smp->ts.origin, &window[i]->ts.origin) >= 0) {
        if (i != window.size() - 1)
          logger->warn("Fixing reordered Sample");

        if (window.size() == window_size) {
          Sample *window_sample = window.front();
          std::copy(++std::begin(window), std::next(std::begin(window), i + 1),
                    std::begin(window));
          window[i] = window_sample;
          swapSample(window_sample, smp);

          return Hook::Reason::OK;
        } else {
          window.push_back(nullptr);
          std::copy_backward(std::next(std::begin(window), i + 1),
                             --std::end(window), std::end(window));
          window[i + 1] = sample_clone(smp);

          logger->debug("window.size={}/{}", window.size(), window_size);

          return Hook::Reason::SKIP_SAMPLE;
        }
      }

      if (!i)
        break;
    }

    logger->error("Could not reorder Sample");

    return Hook::Reason::SKIP_SAMPLE;
  }

  virtual void restart() {
    assert(state == State::STARTED);

    for (auto sample : window)
      sample_free(sample);

    window.clear();
  }
};

// Register hook
static char n[] = "reorder_ts";
static char d[] = "Reorder messages by their timestamp";
static HookPlugin<ReorderTsHook, n, d,
                  (int)Hook::Flags::NODE_WRITE | (int)Hook::Flags::PATH |
                      (int)Hook::Flags::NODE_READ,
                  2>
    p;

} // namespace node
} // namespace villas
