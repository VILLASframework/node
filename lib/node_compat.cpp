/* Legacy nodes.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/node_compat_type.hpp>

using namespace villas;
using namespace villas::node;

NodeCompat::NodeCompat(struct NodeCompatType *vt, const uuid_t &id,
                       const std::string &name)
    : Node(id, name), _vt(vt) {
  _vd = new char[_vt->size];
  if (!_vd)
    throw MemoryAllocationError();

  memset(_vd, 0, _vt->size);

  int ret = _vt->init ? _vt->init(this) : 0;
  if (ret)
    throw RuntimeError("Failed to initialize node");
}

NodeCompat::NodeCompat(const NodeCompat &n) : Node(n) {
  _vd = new char[_vt->size];
  if (!_vd)
    throw MemoryAllocationError();

  memcpy(_vd, n._vd, _vt->size);

  int ret = _vt->init ? _vt->init(this) : 0;
  if (ret)
    throw RuntimeError("Failed to initialize node");
}

NodeCompat &NodeCompat::operator=(const NodeCompat &n) {
  if (&n != this) {
    *this = n;

    _vd = new char[_vt->size];
    if (!_vd)
      throw MemoryAllocationError();

    memcpy(_vd, n._vd, _vt->size);
  }

  return *this;
}

NodeCompat::~NodeCompat() {
  assert(state == State::STOPPED || state == State::PREPARED ||
         state == State::CHECKED || state == State::PARSED ||
         state == State::INITIALIZED);

  int ret __attribute__((unused));
  ret = _vt->destroy ? _vt->destroy(this) : 0;

  delete[](char *) _vd;
}

int NodeCompat::prepare() {
  assert(state == State::CHECKED);

  int ret = _vt->prepare ? _vt->prepare(this) : 0;
  if (ret)
    return ret;

  return Node::prepare();
}

int NodeCompat::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  ret = _vt->parse ? _vt->parse(this, json) : 0;
  if (!ret)
    state = State::PARSED;

  return ret;
}

int NodeCompat::check() {
  int ret = Node::check();
  if (ret)
    return ret;

  ret = _vt->check ? _vt->check(this) : 0;
  if (!ret)
    state = State::CHECKED;

  return ret;
}

int NodeCompat::pause() {
  int ret;

  ret = Node::pause();
  if (ret)
    return ret;

  ret = _vt->pause ? _vt->pause(this) : 0;
  if (!ret)
    state = State::PAUSED;

  return ret;
}

int NodeCompat::resume() {
  int ret;

  ret = Node::resume();
  if (ret)
    return ret;

  ret = _vt->resume ? _vt->resume(this) : 0;
  if (!ret)
    state = State::STARTED;

  return ret;
}

int NodeCompat::restart() {
  return _vt->restart ? _vt->restart(this) : Node::restart();
}

int NodeCompat::_read(struct Sample *smps[], unsigned cnt) {
  return _vt->read ? _vt->read(this, smps, cnt) : -1;
}

int NodeCompat::_write(struct Sample *smps[], unsigned cnt) {
  return _vt->write ? _vt->write(this, smps, cnt) : -1;
}

/* Reverse local and remote socket address.
 *
 * @see node_type::reverse
 */
int NodeCompat::reverse() { return _vt->reverse ? _vt->reverse(this) : -1; }

std::vector<int> NodeCompat::getPollFDs() {
  if (_vt->poll_fds) {
    int ret, fds[16];

    ret = _vt->poll_fds(this, fds);
    if (ret < 0)
      return {};

    return std::vector<int>(fds, fds + ret);
  }

  return {};
}

std::vector<int> NodeCompat::getNetemFDs() {
  if (_vt->netem_fds) {
    int ret, fds[16];

    ret = _vt->netem_fds(this, fds);
    if (ret < 0)
      return {};

    return std::vector<int>(fds, fds + ret);
  }

  return {};
}

struct memory::Type *NodeCompat::getMemoryType() {
  return _vt->memory_type ? _vt->memory_type(this, memory::default_type)
                          : memory::default_type;
}

int NodeCompat::start() {
  assert(state == State::PREPARED || state == State::PAUSED);

  int ret = _vt->start ? _vt->start(this) : 0;
  if (ret)
    return ret;

  ret = Node::start();
  if (!ret)
    state = State::STARTED;

  return ret;
}

int NodeCompat::stop() {
  assert(state == State::STARTED || state == State::PAUSED ||
         state == State::STOPPING);

  int ret = Node::stop();
  if (ret)
    return ret;

  ret = _vt->stop ? _vt->stop(this) : 0;
  if (!ret)
    state = State::STOPPED;

  return ret;
}

const std::string &NodeCompat::getDetails() {
  if (_vt->print && _details.empty()) {
    auto *d = _vt->print(this);
    _details = std::string(d);
    free(d);
  }

  return _details;
}

Node *NodeCompatFactory::make(const uuid_t &id, const std::string &name) {
  auto *n = new NodeCompat(_vt, id, name);

  init(n);

  return n;
}

int NodeCompatFactory::start(SuperNode *sn) {
  assert(state == State::INITIALIZED);

  int ret = _vt->type.start ? _vt->type.start(sn) : 0;
  if (ret)
    return ret;

  return NodeFactory::start(sn);
}

int NodeCompatFactory::stop() {
  assert(state == State::STARTED);

  int ret = _vt->type.stop ? _vt->type.stop() : 0;
  if (ret)
    return ret;

  return NodeFactory::stop();
}

int NodeCompatFactory::getFlags() const {
  int flags = _vt->flags;

  if (_vt->read)
    flags |= (int)NodeFactory::Flags::SUPPORTS_READ;

  if (_vt->write)
    flags |= (int)NodeFactory::Flags::SUPPORTS_WRITE;

  if (_vt->poll_fds)
    flags |= (int)NodeFactory::Flags::SUPPORTS_POLL;

  return flags;
}
