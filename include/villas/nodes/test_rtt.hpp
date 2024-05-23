/* Node type: Node-type for testing Round-trip Time.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/list.hpp>
#include <villas/node.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class TestRTT : public Node {

protected:
  class Case {
    friend TestRTT;

  protected:
    TestRTT *node;

    int id;
    double rate;
    unsigned values;
    unsigned limit; // The number of samples we take per test.

    std::string filename;
    std::string filename_formatted;

  public:
    Case(TestRTT *n, int id, int rate, int values, int limit,
         const std::string & filename)
        : node(n), id(id), rate(rate), values(values), limit(limit),
          filename(filename){};

    int start();
    int stop();
  };

  Task task;             // The periodic task for test_rtt_read()
  Format::Ptr formatter; // The format of the output file
  FILE *stream;

  double cooldown; // Number of seconds to wait between tests.

  unsigned counter;

  std::list<Case> cases; // List of test cases
  std::list<Case>::iterator current_case;

  std::string output; // The directory where we place the results.
  std::string prefix; // An optional prefix in the filename.

  virtual int _read(struct Sample *smps[], unsigned cnt);

  virtual int _write(struct Sample *smps[], unsigned cnt);

public:
  TestRTT(const uuid_t &id = {}, const std::string &name = "")
      : Node(id, name), task(CLOCK_MONOTONIC), formatter(nullptr), stream(nullptr), cooldown(0), counter(-1) {}

  virtual ~TestRTT(){};

  virtual int prepare();

  virtual int parse(json_t *json);

  virtual int start();

  virtual int stop();

  virtual std::vector<int> getPollFDs();

  virtual const std::string &getDetails();
};

} // namespace node
} // namespace villas
