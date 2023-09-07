/* Statistic collection.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <jansson.h>

#include <memory>
#include <string>
#include <unordered_map>

#include <villas/common.hpp>
#include <villas/hist.hpp>
#include <villas/log.hpp>
#include <villas/signal.hpp>
#include <villas/table.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;
class Node;

} // namespace node

class Stats {

public:
  using Ptr = std::shared_ptr<Stats>;

  enum class Format { HUMAN, JSON, MATLAB };

  enum class Metric {
    SMPS_SKIPPED,   // Counter for skipped samples due to hooks.
    SMPS_REORDERED, // Counter for reordered samples.

    // Timings
    GAP_SAMPLE, // Histogram for inter sample timestamps (as sent by remote).
    GAP_RECEIVED, // Histogram for inter sample arrival time (as seen by this instance).
    OWD,          // Histogram for one-way-delay (OWD) of received samples.
    AGE,          // Processing time of packets within VILLASnode.
    SIGNAL_COUNT, // Number of signals per sample.

    // RTP metrics
    RTP_LOSS_FRACTION, // Fraction lost since last RTP SR/RR.
    RTP_PKTS_LOST,     // Cumul. no. pkts lost.
    RTP_JITTER         // Interarrival jitter.
  };

  enum class Type { LAST, HIGHEST, LOWEST, MEAN, VAR, STDDEV, TOTAL };

protected:
  std::unordered_map<Metric, villas::Hist> histograms;

  struct MetricDescription {
    const char *name;
    const char *unit;
    const char *desc;
  };

  struct TypeDescription {
    const char *name;
    enum node::SignalType signal_type;
  };

  static std::shared_ptr<Table> table;

  static void setupTable();

  Logger logger;

public:
  Stats(int buckets, int warmup);

  static enum Format lookupFormat(const std::string &str);

  static enum Metric lookupMetric(const std::string &str);

  static enum Type lookupType(const std::string &str);

  void update(enum Metric id, double val);

  void reset();

  json_t *toJson() const;

  static void printHeader(enum Format fmt);

  void printPeriodic(FILE *f, enum Format fmt, node::Node *n) const;

  void print(FILE *f, enum Format fmt, int verbose) const;

  union node::SignalData getValue(enum Metric sm, enum Type st) const;

  const Hist &getHistogram(enum Metric sm) const;

  static std::unordered_map<Metric, MetricDescription> metrics;
  static std::unordered_map<Type, TypeDescription> types;
  static std::vector<TableColumn> columns;
};

} // namespace villas
