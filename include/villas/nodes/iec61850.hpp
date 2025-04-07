/* Some helpers to libiec61850.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <netinet/ether.h>

#include <libiec61850/goose_receiver.h>
#include <libiec61850/hal_ethernet.h>
#include <libiec61850/sv_subscriber.h>

#include <villas/list.hpp>
#include <villas/signal.hpp>
#include <villas/signal_list.hpp>

#ifndef CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_GOOSE_DEFAULT_DST_ADDRESS {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01}
#endif

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

enum class IEC61850Type {
  // According to IEC 61850-7-2
  BOOLEAN,
  INT8,
  INT16,
  INT32,
  INT64,
  INT8U,
  INT16U,
  INT32U,
  INT64U,
  FLOAT32,
  FLOAT64,
  ENUMERATED,
  CODED_ENUM,
  OCTET_STRING,
  VISIBLE_STRING,
  OBJECTNAME,
  OBJECTREFERENCE,
  TIMESTAMP,
  ENTRYTIME,

  // According to IEC 61850-8-1
  BITSTRING
};

struct iec61850_type_descriptor {
  const char *name;
  enum IEC61850Type iec_type;
  enum SignalType type;
  unsigned size;
  bool publisher;
  bool subscriber;
};

struct iec61850_receiver {
  char *interface;

  enum class Type { GOOSE, SAMPLED_VALUES } type;

  union {
    SVReceiver sv;
    GooseReceiver goose;
  };
};

int iec61850_type_start(villas::node::SuperNode *sn);

int iec61850_type_stop();

const struct iec61850_type_descriptor *iec61850_lookup_type(const char *name);

int iec61850_parse_signals(json_t *json_signals, struct List *signals,
                           SignalList::Ptr node_signals);

struct iec61850_receiver *
iec61850_receiver_lookup(enum iec61850_receiver::Type t, const char *intf);

struct iec61850_receiver *
iec61850_receiver_create(enum iec61850_receiver::Type t, const char *intf,
                         bool check_dst_address);

int iec61850_receiver_start(struct iec61850_receiver *r);

int iec61850_receiver_stop(struct iec61850_receiver *r);

int iec61850_receiver_destroy(struct iec61850_receiver *r);

const struct iec61850_type_descriptor *iec61850_lookup_type(const char *name);

} // namespace node
} // namespace villas
