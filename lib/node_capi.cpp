/* Node C-API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/node.hpp>

extern "C" {
#include <villas/node.h>
}

using namespace villas;
using namespace villas::node;

vnode *node_new(const char *id_str, const char *json_str) {
  json_error_t err;
  uuid_t id;
  uuid_parse(id_str, id);
  auto *json = json_loads(json_str, 0, &err);
  return (vnode *)NodeFactory::make(json, id);
}

int node_prepare(vnode *n) {
  auto *nc = (Node *)n;
  return nc->prepare();
}

int node_check(vnode *n) {
  auto *nc = (Node *)n;
  return nc->check();
}

int node_start(vnode *n) {
  auto *nc = (Node *)n;
  return nc->start();
}

int node_stop(vnode *n) {
  auto *nc = (Node *)n;
  return nc->stop();
}

int node_pause(vnode *n) {
  auto *nc = (Node *)n;
  return nc->pause();
}

int node_resume(vnode *n) {
  auto *nc = (Node *)n;
  return nc->resume();
}

int node_restart(vnode *n) {
  auto *nc = (Node *)n;
  return nc->restart();
}

int node_destroy(vnode *n) {
  auto *nc = (Node *)n;
  nc->~Node();
  return 0;
}

const char *node_name(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getName().c_str();
}

const char *node_name_short(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getNameShort().c_str();
}

const char *node_name_full(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getNameFull().c_str();
}

const char *node_details(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getDetails().c_str();
}

unsigned node_input_signals_max_cnt(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getInputSignalsMaxCount();
}

unsigned node_output_signals_max_cnt(vnode *n) {
  auto *nc = (Node *)n;
  return nc->getOutputSignalsMaxCount();
}

int node_reverse(vnode *n) {
  auto *nc = (Node *)n;
  return nc->reverse();
}

int node_read(vnode *n, vsample **smps, unsigned cnt) {
  auto *nc = (Node *)n;
  return nc->read((villas::node::Sample **)smps, cnt);
}

int node_write(vnode *n, vsample **smps, unsigned cnt) {
  auto *nc = (Node *)n;
  return nc->write((villas::node::Sample **)smps, cnt);
}

int node_poll_fds(vnode *n, int fds[]) {
  auto *nc = (Node *)n;
  auto l = nc->getPollFDs();

  for (unsigned i = 0; i < l.size() && i < 16; i++)
    fds[i] = l[i];

  return l.size();
}

int node_netem_fds(vnode *n, int fds[]) {
  auto *nc = (Node *)n;
  auto l = nc->getNetemFDs();

  for (unsigned i = 0; i < l.size() && i < 16; i++)
    fds[i] = l[i];

  return l.size();
}

bool node_is_valid_name(const char *name) { return Node::isValidName(name); }

bool node_is_enabled(const vnode *n) {
  auto *nc = (Node *)n;
  return nc->isEnabled();
}

json_t *node_to_json(const vnode *n) {
  auto *nc = (Node *)n;
  return nc->toJson();
}

const char *node_to_json_str(vnode *n) {
  auto json = node_to_json(n);
  return json_dumps(json, 0);
}

vsample *sample_alloc(unsigned len) { return (vsample *)sample_alloc_mem(len); }

unsigned sample_length(vsample *s) {
  auto *smp = (Sample *)s;
  return smp->length;
}

vsample *sample_pack(unsigned seq, struct timespec *ts_origin,
                     struct timespec *ts_received, unsigned len,
                     double *values) {
  auto *smp = sample_alloc_mem(len);

  smp->sequence = seq;
  smp->ts.origin = *ts_origin;
  smp->ts.received = *ts_received;
  smp->length = len;
  smp->flags = (int)SampleFlags::HAS_SEQUENCE | (int)SampleFlags::HAS_DATA |
               (int)SampleFlags::HAS_TS_ORIGIN;

  memcpy((double *)smp->data, values, sizeof(double) * len);

  return (vsample *)smp;
}

void sample_unpack(vsample *s, unsigned *seq, struct timespec *ts_origin,
                   struct timespec *ts_received, int *flags, unsigned *len,
                   double *values) {
  auto *smp = (Sample *)s;

  *seq = smp->sequence;

  *ts_origin = smp->ts.origin;
  *ts_received = smp->ts.received;

  *flags = smp->flags;
  *len = smp->length;

  memcpy(values, (double *)smp->data, sizeof(double) * *len);
}

void sample_decref(vsample *smp) { sample_decref((Sample *)smp); }

int memory_init(int hugepages) { return memory::init(hugepages); }
