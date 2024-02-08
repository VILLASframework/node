/* Message paths.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <bitset>

#include <fmt/ostream.h>
#include <jansson.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <villas/colors.hpp>
#include <villas/common.hpp>
#include <villas/config.hpp>
#include <villas/list.hpp>
#include <villas/log.hpp>
#include <villas/mapping_list.hpp>
#include <villas/node.hpp>
#include <villas/node_list.hpp>
#include <villas/path_destination.hpp>
#include <villas/path_source.hpp>
#include <villas/pool.hpp>
#include <villas/queue.h>
#include <villas/signal_list.hpp>
#include <villas/task.hpp>

// Forward declarations
struct pollfd;

namespace villas {
namespace node {

// Forward declarations
class Node;

// The datastructure for a path.
class Path {
  friend PathSource;
  friend SecondaryPathSource;
  friend PathDestination;

protected:
  void *runSingle();
  void *runPoll();

  static void *runWrapper(void *arg);

  void startPoll();

  static int id;

public:
  enum State state; // Path state.

  // The register mode determines under which condition the path is triggered.
  enum class Mode {
    ANY, // The path is triggered whenever one of the sources receives samples.
    ALL // The path is triggered only after all sources have received at least 1 sample.
  } mode; // Determines when this path is triggered.

  uuid_t uuid;

  std::vector<struct pollfd> pfds;

  struct Pool pool;
  struct Sample *last_sample;
  int last_sequence;

  NodeList masked;
  MappingList mappings;             // List of all input mappings.
  PathSourceList sources;           // List of all incoming nodes.
  PathDestinationList destinations; // List of all outgoing nodes.
  HookList hooks;                   // List of processing hooks.
  SignalList::Ptr signals;          // List of signals which this path creates.

  struct Task timeout;

  double rate;              // A timeout for
  int affinity;             // Thread affinity.
  bool enabled;             // Is this path enabled?
  int poll;                 // Weather or not to use poll(2).
  bool reversed;            // This path has a matching reverse path.
  bool builtin;             // This path should use built-in hooks by default.
  int original_sequence_no; // Use original source sequence number when multiplexing
  unsigned queuelen;        // The queue length for each path_destination::queue

  pthread_t tid;  // The thread id for this path.
  json_t *config; // A JSON object containing the configuration of the path.

  Logger logger;

  std::bitset<MAX_SAMPLE_LENGTH>
      mask; // A mask of PathSources which are enabled for poll().
  std::bitset<MAX_SAMPLE_LENGTH>
      received; // A mask of PathSources for which we already received samples.

  friend std::ostream &operator<<(std::ostream &os, const Path &p) {
    if (p.sources.size() > 1)
      os << "[ ";

    for (auto ps : p.sources)
      os << ps->getNode()->getNameShort() << " ";

    if (p.sources.size() > 1)
      os << "] ";

    os << CLR_MAG("=>");

    if (p.destinations.size() != 1)
      os << " [";

    for (auto pd : p.destinations)
      os << " " << pd->getNode()->getNameShort();

    if (p.destinations.size() != 1)
      os << " ]";

    return os;
  }

  std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  Path();
  ~Path();

  void prepare(NodeList &nodes);

  // Check if path configuration is proper.
  void check();

  // Check prepared path.
  void checkPrepared();

  /* Start a path.
	*
	* Start a new pthread for receiving/sending messages over this path.
	*/
  void start();

  // Stop a path.
  void stop();

  // Get a list of signals which is emitted by the path.
  SignalList::Ptr getOutputSignals(bool after_hooks = true);

  unsigned getOutputSignalsMaxCount();

  /* Parse a single path and add it to the global configuration.
	*
	* @param json A JSON object containing the configuration of the path.
	* @param p Pointer to the allocated memory for this path
	* @param nodes A linked list of all existing nodes
	* @retval 0 Success. Everything went well.
	* @retval <0 Error. Something went wrong.
	*/
  void parse(json_t *json, NodeList &nodes, const uuid_t sn_uuid);

  void parseMask(json_t *json_mask, NodeList &nodes);

  bool isSimple() const;
  bool isMuxed() const;

  bool isEnabled() const { return enabled; }

  bool isReversed() const { return reversed; }

  State getState() const { return state; }

  // Get the UUID of this path.
  const uuid_t &getUuid() const { return uuid; }

  json_t *toJson() const;
};

} // namespace node
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::node::Path> : public fmt::ostream_formatter {};
#endif
