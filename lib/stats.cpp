/* Statistic collection.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/hist.hpp>
#include <villas/node.hpp>
#include <villas/stats.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

std::unordered_map<Stats::Metric, Stats::MetricDescription> Stats::metrics = {
    {Stats::Metric::SMPS_SKIPPED,
     {"skipped", "samples", "Skipped samples and the distance between them"}},
    {Stats::Metric::SMPS_REORDERED,
     {"reordered", "samples",
      "Reordered samples and the distance between them"}},
    {Stats::Metric::GAP_SAMPLE,
     {"gap_sent", "seconds", "Inter-message timestamps (as sent by remote)"}},
    {Stats::Metric::GAP_RECEIVED,
     {"gap_received", "seconds",
      "Inter-message arrival time (as received by this instance)"}},
    {Stats::Metric::OWD,
     {"owd", "seconds", "One-way-delay (OWD) of received messages"}},
    {Stats::Metric::AGE,
     {"age", "seconds",
      "Processing time of packets within the from receive to sent"}},
    {Stats::Metric::SIGNAL_COUNT,
     {"signal_cnt", "signals", "Number of signals per sample"}},
    {Stats::Metric::RTP_LOSS_FRACTION,
     {"rtp.loss_fraction", "percent", "Fraction lost since last RTP SR/RR."}},
    {Stats::Metric::RTP_PKTS_LOST,
     {"rtp.pkts_lost", "packets", "Cumulative number of packets lost"}},
    {Stats::Metric::RTP_JITTER,
     {"rtp.jitter", "seconds", "Interarrival jitter"}},
};

std::unordered_map<Stats::Type, Stats::TypeDescription> Stats::types = {
    {Stats::Type::LAST, {"last", SignalType::FLOAT}},
    {Stats::Type::HIGHEST, {"highest", SignalType::FLOAT}},
    {Stats::Type::LOWEST, {"lowest", SignalType::FLOAT}},
    {Stats::Type::MEAN, {"mean", SignalType::FLOAT}},
    {Stats::Type::VAR, {"var", SignalType::FLOAT}},
    {Stats::Type::STDDEV, {"stddev", SignalType::FLOAT}},
    {Stats::Type::TOTAL, {"total", SignalType::INTEGER}}};

std::vector<TableColumn> Stats::columns = {
    {10, TableColumn::Alignment::LEFT, "Node", "%s"},
    {10, TableColumn::Alignment::RIGHT, "Recv", "%ju", "pkts"},
    {10, TableColumn::Alignment::RIGHT, "Sent", "%ju", "pkts"},
    {10, TableColumn::Alignment::RIGHT, "Drop", "%ju", "pkts"},
    {10, TableColumn::Alignment::RIGHT, "Skip", "%ju", "pkts"},
    {10, TableColumn::Alignment::RIGHT, "OWD last", "%lf", "secs"},
    {10, TableColumn::Alignment::RIGHT, "OWD mean", "%lf", "secs"},
    {10, TableColumn::Alignment::RIGHT, "Rate last", "%lf", "pkt/sec"},
    {10, TableColumn::Alignment::RIGHT, "Rate mean", "%lf", "pkt/sec"},
    {10, TableColumn::Alignment::RIGHT, "Age mean", "%lf", "secs"},
    {10, TableColumn::Alignment::RIGHT, "Age Max", "%lf", "sec"},
    {8, TableColumn::Alignment::RIGHT, "Signals", "%ju", "signals"}};

enum Stats::Format Stats::lookupFormat(const std::string &str) {
  if (str == "human")
    return Format::HUMAN;
  else if (str == "json")
    return Format::JSON;
  else if (str == "matlab")
    return Format::MATLAB;

  throw std::invalid_argument("Invalid format");
}

enum Stats::Metric Stats::lookupMetric(const std::string &str) {
  for (auto m : metrics) {
    if (str == m.second.name)
      return m.first;
  }

  throw std::invalid_argument("Invalid stats metric");
}

enum Stats::Type Stats::lookupType(const std::string &str) {
  for (auto t : types) {
    if (str == t.second.name)
      return t.first;
  }

  throw std::invalid_argument("Invalid stats type");
}

Stats::Stats(int buckets, int warmup) : logger(logging.get("stats")) {
  for (auto m : metrics) {
    histograms.emplace(std::piecewise_construct, std::forward_as_tuple(m.first),
                       std::forward_as_tuple(buckets, warmup));
  }
}

void Stats::update(enum Metric m, double val) { histograms[m].put(val); }

void Stats::reset() {
  for (auto m : metrics)
    histograms[m.first].reset();
}

json_t *Stats::toJson() const {
  json_t *obj = json_object();

  for (auto m : metrics) {
    const Hist &h = histograms.at(m.first);

    json_object_set_new(obj, m.second.name, h.toJson());
  }

  return obj;
}

void Stats::printHeader(enum Format fmt) {
  switch (fmt) {
  case Format::HUMAN:
    setupTable();
    table->header();
    break;

  default: {
  }
  }
}

void Stats::setupTable() {
  if (!table) {
    auto logger = logging.get("stats");
    table = std::make_shared<Table>(logger, columns);
  }
}

void Stats::printPeriodic(FILE *f, enum Format fmt, node::Node *n) const {
  switch (fmt) {
  case Format::HUMAN:
    setupTable();
    table->row(11, n->getNameShort().c_str(),
               (uintmax_t)histograms.at(Metric::OWD).getTotal(),
               (uintmax_t)histograms.at(Metric::AGE).getTotal(),
               (uintmax_t)histograms.at(Metric::SMPS_REORDERED).getTotal(),
               (uintmax_t)histograms.at(Metric::SMPS_SKIPPED).getTotal(),
               (double)histograms.at(Metric::OWD).getLast(),
               (double)histograms.at(Metric::OWD).getMean(),
               (double)1.0 / histograms.at(Metric::GAP_RECEIVED).getLast(),
               (double)1.0 / histograms.at(Metric::GAP_RECEIVED).getMean(),
               (double)histograms.at(Metric::AGE).getMean(),
               (double)histograms.at(Metric::AGE).getHighest(),
               (uintmax_t)histograms.at(Metric::SIGNAL_COUNT).getLast());
    break;

  case Format::JSON: {
    json_t *json_stats = json_pack(
        "{ s: s, s: i, s: i, s: i, s: i, s: f, s: f, s: f, s: f, s: f, s: f, "
        "s: i }",
        "node", n->getNameShort().c_str(), "recv",
        histograms.at(Metric::OWD).getTotal(), "sent",
        histograms.at(Metric::AGE).getTotal(), "dropped",
        histograms.at(Metric::SMPS_REORDERED).getTotal(), "skipped",
        histograms.at(Metric::SMPS_SKIPPED).getTotal(), "owd_last",
        1.0 / histograms.at(Metric::OWD).getLast(), "owd_mean",
        1.0 / histograms.at(Metric::OWD).getMean(), "rate_last",
        1.0 / histograms.at(Metric::GAP_SAMPLE).getLast(), "rate_mean",
        1.0 / histograms.at(Metric::GAP_SAMPLE).getMean(), "age_mean",
        histograms.at(Metric::AGE).getMean(), "age_max",
        histograms.at(Metric::AGE).getHighest(), "signals",
        histograms.at(Metric::SIGNAL_COUNT).getLast());
    json_dumpf(json_stats, f, 0);
    break;
  }

  default: {
  }
  }
}

void Stats::print(FILE *f, enum Format fmt, int verbose) const {
  switch (fmt) {
  case Format::HUMAN:
    for (auto m : metrics) {
      logger->info("{}: {}", m.second.name, m.second.desc);
      histograms.at(m.first).print(logger, verbose);
    }
    break;

  case Format::JSON:
    json_dumpf(toJson(), f, 0);
    fflush(f);
    break;

  default: {
  }
  }
}

union SignalData Stats::getValue(enum Metric sm, enum Type st) const {
  const Hist &h = histograms.at(sm);
  union SignalData d;

  switch (st) {
  case Type::TOTAL:
    d.i = h.getTotal();
    break;

  case Type::LAST:
    d.f = h.getLast();
    break;

  case Type::HIGHEST:
    d.f = h.getHighest();
    break;

  case Type::LOWEST:
    d.f = h.getLowest();
    break;

  case Type::MEAN:
    d.f = h.getMean();
    break;

  case Type::STDDEV:
    d.f = h.getStddev();
    break;

  case Type::VAR:
    d.f = h.getVar();
    break;

  default:
    d.f = -1;
  }

  return d;
}

const Hist &Stats::getHistogram(enum Metric sm) const {
  return histograms.at(sm);
}

std::shared_ptr<Table> Stats::table = std::shared_ptr<Table>();
