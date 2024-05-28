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
    double
        warmup; // Number of seconds to wait between before recording samples.
    double cooldown; // Number of seconds to wait between tests.
    unsigned values;

    unsigned count; // The number of samples we send per test.
    unsigned sent;
    unsigned received;
    unsigned missed;

    unsigned count_warmup; // The number of samples we send during warmup.
    unsigned sent_warmup;
    unsigned received_warmup;
    unsigned missed_warmup;

    struct timespec started;
    struct timespec stopped;

    std::string filename;
    std::string filename_formatted;

    json_t *getMetadata();

  public:
    Case(TestRTT *n, int id, int rate, float warmup, float cooldown, int values,
         int count, int count_warmup, const std::string &filename)
        : node(n), id(id), rate(rate), warmup(warmup), cooldown(cooldown),
          values(values), count(count), sent(0), received(0), missed(0),
          count_warmup(count_warmup), sent_warmup(0), received_warmup(0),
          missed_warmup(0), filename(filename){};

    int start();
    int stop();
    double getEstimatedDuration() const;
  };

  Task task;             // The periodic task for test_rtt_read()
  Format::Ptr formatter; // The format of the output file
  FILE *stream;

  std::list<Case> cases;             // List of test cases
  std::list<Case>::iterator current; // Currently running test case

  std::string output; // The directory where we place the results.
  std::string prefix; // An optional prefix in the filename.

  bool shutdown;

  virtual int _read(struct Sample *smps[], unsigned cnt);

  virtual int _write(struct Sample *smps[], unsigned cnt);

public:
  enum Mode {
    UNKNOWN,
    MIN,
    MAX,
    STOP_COUNT,
    STOP_DURATION,
    AT_LEAST_COUNT,
    AT_LEAST_DURATION
  };

  TestRTT(const uuid_t &id = {}, const std::string &name = "")
      : Node(id, name), task(CLOCK_MONOTONIC), formatter(nullptr),
        stream(nullptr), shutdown(false) {}

  virtual ~TestRTT(){};

  virtual int prepare();

  virtual int parse(json_t *json);

  virtual int start();

  virtual int stop();

  virtual std::vector<int> getPollFDs();

  virtual const std::string &getDetails();

  double getEstimatedDuration() const;
};

class TestRTTNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  virtual Node *make(const uuid_t &id = {}, const std::string &nme = "") {
    auto *n = new TestRTT(id, nme);

    init(n);

    return n;
  }

  virtual int getFlags() const {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL;
  }

  virtual std::string getName() const { return "test_rtt"; }

  virtual std::string getDescription() const {
    return "Test round-trip time with loopback";
  }

  virtual int start(SuperNode *sn);
};

} // namespace node
} // namespace villas
