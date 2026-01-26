/* Node type: Node-type for testing Round-trip Time.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstring>

#include <linux/limits.h>
#include <sys/stat.h>

#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/nodes/test_rtt.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static SuperNode *sn = nullptr;

int TestRTT::Case::start() {
  node->logger->info("Starting case #{}/{}: filename={}, rate={}/s, values={}, "
                     "count={}smps, warmup={:.3f}s, cooldown={:.3f}s, "
                     "estimated_duration={:.3f}s",
                     id + 1, node->cases.size(), filename_formatted, rate,
                     values, count, warmup, cooldown, getEstimatedDuration());

  // Open file
  node->stream = fopen(filename_formatted.c_str(), "a+");
  if (!node->stream)
    return -1;

  started = time_now();

  node->formatter->reset();

  // Start timer
  node->task.setRate(rate);

  return 0;
}

int TestRTT::Case::stop() {
  int ret;

  stopped = time_now();

  // Stop timer
  node->task.stop();

  node->formatter->printMetadata(node->stream, getMetadata());

  ret = fclose(node->stream);
  if (ret)
    throw SystemError("Failed to close file");

  node->logger->info("Stopping case #{}/{}: count={}smps, sent={}smps, "
                     "received={}smps, missed={}smps, duration={:.3f}s",
                     id + 1, node->cases.size(), count, sent, received, missed,
                     time_delta(&started, &stopped));

  return 0;
}

json_t *TestRTT::Case::getMetadata() {
  json_t *json_warmup = nullptr;

  if (count_warmup > 0) {
    json_warmup = json_pack("{ s: i, s: i, s: i, s: i }", "count", count_warmup,
                            "sent", sent_warmup, "received", received_warmup,
                            "missed", missed_warmup);
  }

  return json_pack(
      "{ s: i, s: f, s: i, s: f, s: f, s: i, s: i, s: i, s: i, s: o* }", "id",
      id, "rate", rate, "values", values, "started", time_to_double(&started),
      "stopped", time_to_double(&stopped), "count", count, "sent", sent,
      "received", received, "missed", missed, "warmup", json_warmup);
}

double TestRTT::Case::getEstimatedDuration() const {
  return (count_warmup + count) / rate + cooldown;
}

int TestRTT::prepare() {
  unsigned max_values = 0;

  // Take current for time for test case prefix
  time_t ts = time(nullptr);
  struct tm tm;
  gmtime_r(&ts, &tm);

  for (auto &c : cases) {
    if (c.values > max_values)
      max_values = c.values;

    auto filename_formatted = new char[NAME_MAX];
    if (!filename_formatted)
      throw MemoryAllocationError();

    strftime(filename_formatted, NAME_MAX, c.filename.c_str(), &tm);

    c.filename_formatted = filename_formatted;
  }

  in.signals = std::make_shared<SignalList>(max_values, SignalType::FLOAT);

  return Node::prepare();
}

static enum TestRTT::Mode parseMode(const char *mode_str) {
  if (strcmp(mode_str, "min"))
    return TestRTT::Mode::MIN;
  else if (strcmp(mode_str, "max"))
    return TestRTT::Mode::MIN;
  else if (strcmp(mode_str, "stop_after_count"))
    return TestRTT::Mode::STOP_COUNT;
  else if (strcmp(mode_str, "stop_after_duration"))
    return TestRTT::Mode::STOP_DURATION;
  else if (strcmp(mode_str, "at_least_count"))
    return TestRTT::Mode::AT_LEAST_COUNT;
  else if (strcmp(mode_str, "at_least_duration"))
    return TestRTT::Mode::AT_LEAST_DURATION;
  else
    return TestRTT::Mode::UNKNOWN;
}

int TestRTT::parse(json_t *json) {
  int ret;

  const char *output_str = ".";
  const char *prefix_str = nullptr;
  const char *mode_str = nullptr;

  enum Mode mode_default = Mode::AT_LEAST_COUNT;
  int count_default = 1000;
  double duration_default = 300;
  double warmup_default = 0;
  double cooldown_default = 0;
  int shutdown_ = -1;

  size_t i;
  json_t *json_cases, *json_case, *json_val, *json_format = nullptr;
  json_t *json_rates_default = nullptr, *json_values_default = nullptr;
  json_error_t err;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s?: s, s?: s, s?: o, s?: F, s?: F, s?: o, s?: o, s: "
                       "o, s?: b, s?: s, s?: i, s?: F }",
                       "prefix", &prefix_str, "output", &output_str, "format",
                       &json_format, "cooldown", &cooldown_default, "warmup",
                       &warmup_default, "values", &json_values_default, "rates",
                       &json_rates_default, "cases", &json_cases, "shutdown",
                       &shutdown_, "mode", &mode_str, "count", &count_default,
                       "duration", &duration_default);
  if (ret)
    throw ConfigError(json, err, "node-config-node-test-rtt");

  output = output_str;
  prefix = prefix_str ? prefix_str : getNameShort();

  if (shutdown_ > 0)
    shutdown = shutdown_ > 0;

  if (mode_str) {
    mode_default = parseMode(mode_str);
    if (mode_default == Mode::UNKNOWN)
      throw ConfigError(json, "node-config-node-test-rtt-mode",
                        "Invalid mode: {}. Must be 'min' or 'max'", mode_str);
  }

  // Formatter
  auto *fmt = json_format ? FormatFactory::make(json_format)
                          : FormatFactory::make("villas.human");

  formatter = Format::Ptr(fmt);
  if (!formatter)
    throw ConfigError(json_format, "node-config-node-test-rtt-format",
                      "Invalid format configuration");

  if (json_rates_default && !json_is_array(json_rates_default) &&
      !json_is_number(json_rates_default))
    throw ConfigError(
        json_rates_default, "node-config-node-test-rtt-rates",
        "The 'rates' setting must be a real or an array of real numbers");

  if (json_values_default && !json_is_array(json_values_default) &&
      !json_is_integer(json_values_default))
    throw ConfigError(
        json_values_default, "node-config-node-test-rtt-values",
        "The 'values' setting must be an integer or an array of integers");

  // Construct List of test cases
  if (!json_is_array(json_cases))
    throw ConfigError(json_cases, "node-config-node-test-rtt-format",
                      "The 'cases' setting must be an array.");

  int id = 0;
  json_array_foreach (json_cases, i, json_case) {

    const char *mode_str = nullptr;
    int count = count_default;          // in no of samples
    double duration = duration_default; // in secs
    double cooldown = cooldown_default; // in secs
    double warmup = warmup_default;     // in secs
    std::vector<int> rates;
    std::vector<int> values;

    enum Mode mode = mode_default;

    json_t *json_rates = json_rates_default;
    json_t *json_values = json_values_default;

    ret = json_unpack_ex(
        json_case, &err, 0, "{ s?: o, s?: o, s?: i, s?: F, s?: s }", "rates",
        &json_rates, "values", &json_values, "count", &count, "duration",
        &duration, "warmup", &warmup, "cooldown", &cooldown, "mode", &mode_str);

    if (!json_is_array(json_rates) && !json_is_number(json_rates))
      throw ConfigError(
          json_case, "node-config-node-test-rtt-rates",
          "The 'rates' setting must be a real or an array of real numbers");

    if (!json_is_array(json_values) && !json_is_integer(json_values))
      throw ConfigError(
          json_case, "node-config-node-test-rtt-values",
          "The 'values' setting must be an integer or an array of integers");

    if (mode_str) {
      mode = parseMode(mode_str);
      if (mode == Mode::UNKNOWN)
        throw ConfigError(json, "node-config-node-test-rtt-mode",
                          "Invalid mode: {}. Must be 'min' or 'max'", mode_str);
    }

    if (json_is_array(json_rates)) {
      size_t j;
      json_array_foreach (json_rates, j, json_val) {
        if (!json_is_number(json_val))
          throw ConfigError(
              json_val, "node-config-node-test-rtt-rates",
              "The 'rates' setting must be an array of real numbers");

        rates.push_back(json_integer_value(json_val));
      }
    } else
      rates.push_back(json_number_value(json_rates));

    if (json_is_array(json_values)) {
      size_t j;
      json_array_foreach (json_values, j, json_val) {
        if (!json_is_integer(json_val))
          throw ConfigError(
              json_val, "node-config-node-test-rtt-values",
              "The 'values' setting must be an array of integers");

        values.push_back(json_integer_value(json_val));
      }
    } else
      values.push_back(json_integer_value(json_values));

    for (int rate : rates) {
      for (int value : values) {
        int count_effective;
        int count_duration = duration * rate;
        int count_warmup = warmup * rate;

        switch (mode) {
        case Mode::MIN:
          count_effective = MIN(count, count_duration);
          break;

        case Mode::MAX:
          count_effective = MAX(count, count_duration);
          break;

        case Mode::STOP_COUNT:
          count_effective = count_duration;
          if (count_effective > count)
            count_effective = count;
          break;

        case Mode::STOP_DURATION:
          count_effective = count;
          if (count_effective > count_duration)
            count_effective = count_duration;
          break;

        case Mode::AT_LEAST_COUNT:
          count_effective = count_duration;
          if (count_effective < count)
            count_effective = count;
          break;

        case Mode::AT_LEAST_DURATION:
          count_effective = count;
          if (count_effective < count_duration)
            count_effective = count_duration;
          break;

        default: {
          count_effective = 0;
          break;
        }
        }

        auto filename = fmt::format("{}/{}_values{}_rate{}.log", output, prefix,
                                    value, rate);

        cases.emplace_back(this, id++, rate, warmup, cooldown, value,
                           count_effective, count_warmup, filename);
      }
    }
  }

  return 0;
}

const std::string &TestRTT::getDetails() {
  if (details.empty()) {
    details = fmt::format("output={}, prefix={}, #cases={}, shutdown={}, "
                          "estimated_duration={:.3f}s",
                          output, prefix, cases.size(), shutdown,
                          getEstimatedDuration());
  }

  return details;
}

int TestRTT::start() {
  int ret;
  struct stat st;

  // Create output folder for results if not present
  ret = stat(output.c_str(), &st);
  if (ret || !S_ISDIR(st.st_mode)) {
    ret = mkdir(output.c_str(), 0777);
    if (ret)
      throw SystemError("Failed to create output directory: {}", output);
  }

  formatter->start(getInputSignals(false), ~(int)SampleFlags::HAS_DATA);

  current = cases.begin();

  task.setRate(current->rate);

  ret = Node::start();
  if (!ret)
    state = State::STARTED;

  return 0;
}

int TestRTT::stop() {
  int ret;

  if (current != cases.end() && current->received > 0) {
    ret = current->stop();
    if (ret)
      return ret;
  }

  return 0;
}

int TestRTT::_read(struct Sample *smps[], unsigned cnt) {
  int ret;

  // Wait for next sample or cooldown
  auto steps = task.wait();
  if (steps > 1) {
    logger->warn("Skipped {} steps", steps - 1);

    if (current->sent_warmup < current->count_warmup)
      current->missed_warmup += steps - 1;
    else
      current->missed += steps - 1;
  }

  // Cooldown of case completed..
  if (current->sent + current->sent_warmup >=
      current->count + current->count_warmup) {
    ret = current->stop();
    if (ret)
      return ret;

    if (++current == cases.end()) {
      logger->info("This was the last case.");

      setState(State::STOPPING);

      if (sn && shutdown)
        sn->setState(State::STOPPING);

      return -1;
    }
  }

  // Handle start/stop of new cases
  if (current->sent + current->sent_warmup == 0) {
    ret = current->start();
    if (ret)
      return ret;

    if (current->count_warmup > 0)
      logger->info("Starting warmup phase. Sending {} samples...",
                   current->count_warmup);
  } else if (current->sent + current->sent_warmup == current->count_warmup)
    logger->info("Completed warmup phase. Sending {} samples...",
                 current->count);

  auto now = time_now();

  // Prepare samples
  unsigned i;
  for (i = 0; i < cnt; i++) {
    auto smp = smps[i];

    smp->length = current->values;
    smp->sequence = current->sent + current->sent_warmup;
    smp->ts.origin = now;
    smp->flags = (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_SEQUENCE |
                 (int)SampleFlags::HAS_TS_ORIGIN;
    smp->signals = getInputSignals(false);

    if (current->sent_warmup < current->count_warmup)
      current->sent_warmup++;
    else
      current->sent++;
  }

  if (current->sent + current->sent_warmup >=
      current->count + current->count_warmup) {
    if (current->cooldown > 0) {
      logger->info("Entering cooldown phase. Waiting {} seconds...",
                   current->cooldown);
      task.setTimeout(current->cooldown);
    } else {
      task.setTimeout(0); // Start next case immediately
    }
  }

  return i;
}

int TestRTT::_write(struct Sample *smps[], unsigned cnt) {
  if (current == cases.end())
    return 0;

  unsigned i;
  for (i = 0; i < cnt; i++) {
    auto *smp = smps[i];

    if (smp->length != current->values) {
      logger->warn("Discarding invalid sample due to mismatching length: "
                   "expecting={}, has={}",
                   current->values, smp->length);
      continue;
    }

    if (smp->sequence < current->count_warmup) {
      // Skip samples from warmup phase
      current->received_warmup++;
      continue;
    }

    formatter->print(stream, smps[i]);
    current->received++;
  }

  return i;
}

std::vector<int> TestRTT::getPollFDs() { return {task.getFD()}; }

double TestRTT::getEstimatedDuration() const {
  double duration = 0;

  for (auto &c : cases) {
    duration += c.getEstimatedDuration();
  }

  return duration;
}

int TestRTTNodeFactory::start(SuperNode *sn_) {
  sn = sn_;

  return 0;
}

static TestRTTNodeFactory p;
