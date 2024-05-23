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
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int TestRTT::Case::start() {
  node->logger->info(
      "Starting case #{}/{}: filename={}, rate={}, values={}, limit={}", id+1,
      node->cases.size(), filename_formatted, rate, values, limit);

  // Open file
  node->stream = fopen(filename_formatted.c_str(), "a+");
  if (!node->stream)
    return -1;

  // Start timer
  node->task.setRate(rate);

  return 0;
}

int TestRTT::Case::stop() {
  int ret;

  // Stop timer
  node->task.stop();

  ret = fclose(node->stream);
  if (ret)
    throw SystemError("Failed to close file");

  node->logger->info("Stopping case #{}/{}", id+1, node->cases.size());

  return 0;
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

  size_t i;
  json_t *json_cases, *json_case, *json_val, *json_format = nullptr;
  json_t *json_rates = nullptr, *json_values = nullptr;
  json_error_t err;

  cooldown = 0;

  ret =
      json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: o, s?: F, s: o }",
                     "prefix", &prefix_str, "output", &output_str, "format",
                     &json_format, "cooldown", &cooldown, "cases", &json_cases);
  if (ret)
    throw ConfigError(json, err, "node-config-node-test-rtt");

  output = output_str;
  prefix = prefix_str ? prefix_str : getNameShort();

  // Formatter
  auto *fmt = json_format ? FormatFactory::make(json_format)
                          : FormatFactory::make("villas.human");

  formatter = Format::Ptr(fmt);
  if (!formatter)
    throw ConfigError(json_format, "node-config-node-exec-format",
                      "Invalid format configuration");

  // Construct List of test cases
  if (!json_is_array(json_cases))
    throw ConfigError(json_cases, "node-config-node-test-rtt-format",
                      "The 'cases' setting must be an array.");

  int id = 0;
  json_array_foreach(json_cases, i, json_case) {
    int limit = -1;
    double duration = -1; // in secs
    std::vector<int> rates;
    std::vector<int> values;

    ret = json_unpack_ex(json_case, &err, 0, "{ s: o, s: o, s?: i, s?: F }",
                         "rates", &json_rates, "values", &json_values, "limit",
                         &limit, "duration", &duration);

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
        int lim;
        if (limit > 0)
          lim = limit;
        else if (duration > 0)
          lim = duration * rate;
        else
          lim = 1000; // Default value

        auto filename = fmt::format("{}/{}_values{}_rate{}.log", output, prefix,
                                    value, rate);

        cases.emplace_back(this, id++, rate, value, lim, filename);
      }
    }
  }

  return 0;
}

const std::string &TestRTT::getDetails() {
  if (details.empty()) {
    details = fmt::format("output={}, prefix={}, cooldown={}, #cases={}",
                          output, prefix, cooldown, cases.size());
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

  current_case = cases.begin();
  counter = 0;

  task.setRate(current_case->rate);

  ret = Node::start();
  if (!ret)
    state = State::STARTED;

  return 0;
}

int TestRTT::stop() {
  int ret;

  if (counter > 0 && current_case != cases.end()) {
    ret = current_case->stop();
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
  }

  // Cooldown of case completed..
  if (counter >= current_case->limit) {
    ret = current_case->stop();
    if (ret)
      return ret;

    if (++current_case == cases.end()) {
      logger->info("This was the last case.");

      setState(State::STOPPING);

      return -1;
    }

    counter = 0;
  }

  // Handle start/stop of new cases
  if (counter == 0) {
    ret = current_case->start();
    if (ret)
      return ret;
  }

  auto now = time_now();

  // Prepare samples
  unsigned i;
  for (i = 0; i < cnt; i++) {
    auto smp = smps[i];

    smp->length = current_case->values;
    smp->sequence = counter;
    smp->ts.origin = now;
    smp->flags = (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_SEQUENCE |
                 (int)SampleFlags::HAS_TS_ORIGIN;
    smp->signals = getInputSignals(false);

    counter++;
  }

  if (counter >= current_case->limit) {
    if (cooldown) {
      logger->info("Entering cooldown phase. Waiting {} seconds...", cooldown);
      task.setTimeout(cooldown);
    } else {
      task.setTimeout(0); // Start next case immediately
    }
  }

  return i;
}

int TestRTT::_write(struct Sample *smps[], unsigned cnt) {
  if (current_case == cases.end())
    return 0;

  unsigned i;
  for (i = 0; i < cnt; i++) {
    if (smps[i]->length != current_case->values) {
      logger->warn("Discarding invalid sample due to mismatching length: "
                   "expecting={}, has={}",
                   current_case->values, smps[i]->length);
      continue;
    }

    formatter->print(stream, smps[i]);
  }

  return i;
}

std::vector<int> TestRTT::getPollFDs() { return {task.getFD()}; }

// Register node
static char n[] = "test_rtt";
static char d[] = "Test round-trip time with loopback";
static NodePlugin<TestRTT, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL>
    p;
