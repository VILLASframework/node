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
                     "limit={}smps, warmup={}s, cooldown={}s",
                     id + 1, node->cases.size(), filename_formatted, rate,
                     values, limit, warmup, cooldown);

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

  node->logger->info("Stopping case #{}/{}: sent={}, received={}, duration={:.2}", id + 1, node->cases.size(), sent, received, time_delta(&started, &stopped));

  return 0;
}

json_t *TestRTT::Case::getMetadata() {
  json_t *json_warmup = nullptr;

  if (limit_warmup > 0) {
    json_warmup = json_pack("{ s: i, s: i, s: i }", "limit", limit_warmup,
                            "sent", sent_warmup, "received", received_warmup);
  }

  return json_pack(
      "{ s: i, s: f, s: i, s: f, s: f, s: i, s: i, s: i, s: i, s: o* }", "id",
      id, "rate", rate, "values", values, "started", time_to_double(&started),
      "stopped", time_to_double(&stopped), "missed", missed, "limit", limit,
      "sent", sent, "received", received, "warmup", json_warmup);
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

int TestRTT::parse(json_t *json) {
  int ret;

  const char *output_str = ".";
  const char *prefix_str = nullptr;
  double warmup_default = 0;
  double cooldown_default = 0;
  int shutdown_ = -1;

  size_t i;
  json_t *json_cases, *json_case, *json_val, *json_format = nullptr;
  json_t *json_rates_default = nullptr, *json_values_default = nullptr;
  json_error_t err;

  ret = json_unpack_ex(
      json, &err, 0,
      "{ s?: s, s?: s, s?: o, s?: F, s?: F, s?: o, s?: o, s: o, s?: b }",
      "prefix", &prefix_str, "output", &output_str, "format", &json_format,
      "cooldown", &cooldown_default, "warmup", &warmup_default, "values",
      &json_values_default, "rates", &json_rates_default, "cases", &json_cases,
      "shutdown", &shutdown_);
  if (ret)
    throw ConfigError(json, err, "node-config-node-test-rtt");

  output = output_str;
  prefix = prefix_str ? prefix_str : getNameShort();

  if (shutdown_ > 0)
    shutdown = shutdown_ > 0;

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
  json_array_foreach(json_cases, i, json_case) {
    int limit = -1;
    double duration = -1;               // in secs
    double cooldown = cooldown_default; // in secs
    double warmup = warmup_default;     // in secs
    std::vector<int> rates;
    std::vector<int> values;

    json_t *json_rates = json_rates_default;
    json_t *json_values = json_values_default;

    ret = json_unpack_ex(json_case, &err, 0, "{ s?: o, s?: o, s?: i, s?: F }",
                         "rates", &json_rates, "values", &json_values, "limit",
                         &limit, "duration", &duration, "warmup", &warmup,
                         "cooldown", &cooldown);

    if (limit > 0 && duration > 0)
      throw ConfigError(
          json_case, "node-config-node-test-rtt-duration",
          "The settings 'duration' and 'limit' must be used exclusively");

    if (!json_is_array(json_rates) && !json_is_number(json_rates))
      throw ConfigError(
          json_case, "node-config-node-test-rtt-rates",
          "The 'rates' setting must be a real or an array of real numbers");

    if (!json_is_array(json_values) && !json_is_integer(json_values))
      throw ConfigError(
          json_case, "node-config-node-test-rtt-values",
          "The 'values' setting must be an integer or an array of integers");

    if (json_is_array(json_rates)) {
      size_t j;
      json_array_foreach(json_rates, j, json_val) {
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
      json_array_foreach(json_values, j, json_val) {
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
        int lim, lim_warmup;
        if (limit > 0)
          lim = limit;
        else if (duration > 0)
          lim = duration * rate;
        else
          lim = 1000; // Default value

        lim_warmup = warmup * rate;

        auto filename = fmt::format("{}/{}_values{}_rate{}.log", output, prefix,
                                    value, rate);

        cases.emplace_back(this, id++, rate, warmup, cooldown, value, lim,
                           lim_warmup, filename);
      }
    }
  }

  return 0;
}

const std::string &TestRTT::getDetails() {
  if (details.empty()) {
    details = fmt::format("output={}, prefix={}, #cases={}, shutdown={}", output, prefix,
                          cases.size(), shutdown);
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
    current->missed += steps - 1;
  }

  // Cooldown of case completed..
  if (current->sent + current->sent_warmup >=
      current->limit + current->limit_warmup) {
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

    if (current->limit_warmup > 0)
      logger->info("Starting warmup phase. Sending {} samples...",
                   current->limit_warmup);
  } else if (current->sent == current->limit_warmup)
    logger->info("Completed warmup phase. Sending {} samples...",
                 current->limit);

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

    if (current->sent_warmup < current->limit_warmup)
      current->sent_warmup++;
    else
      current->sent++;
  }

  if (current->sent + current->sent_warmup >=
      current->limit + current->limit_warmup) {
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

    if (smp->sequence < current->limit_warmup) {
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

int TestRTTNodeFactory::start(SuperNode *sn_) {
  sn = sn_;

  return 0;
}

static TestRTTNodeFactory p;
