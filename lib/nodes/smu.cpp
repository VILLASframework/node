/* Node-type for SMU data acquisition.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <atomic>
#include <byteswap.h>
#include <cmath>
#include <cstring>
#include <list>

#include <villas/exceptions.hpp>
#include <villas/nodes/smu.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SMUNode::SMUNode(const uuid_t &id, const std::string &name)
    : Node(id, name), mode(MODE_FREERUN), sample_rate(FS_10kSPS), fps(FB_10FPS),
      sync(SYNC_PPS), daq_cfg({sample_rate, fps, mode, sync}),
      shared_mem(nullptr), // initalization
      shared_mem_pos(0),   // initalization
      shared_mem_inc(0),   // initalization
      shared_mem_dim(0),   // initalization
      buffer_size(0) {
  return;
}

int SMUNode::prepare() {
  assert(state == State::CHECKED);
  return Node::prepare();
}

int SMUNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  json_error_t err;

  json_t *jsonDumpers = nullptr;

  char *modeIn = nullptr;
  char *syncIn = nullptr;
  char *fpsIn = nullptr;
  char *sample_rateIn = nullptr;
  json_t *in_json = nullptr;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s?: s, s?: s, s?: s, s?: s, s?: o, s?: o }", "mode",
                       &modeIn, "sync", &syncIn, "fps", &fpsIn, "sample_rate",
                       &sample_rateIn, "dumper", &jsonDumpers, "in", &in_json);
  if (ret)
    throw ConfigError(json, err, "node-config-node-smu");

  if (jsonDumpers) {
    auto signals = getInputSignals();

    size_t i;
    json_t *jsonDumper;
    ret = json_unpack_ex(jsonDumpers, &err, 0, "{ o }", &jsonDumper);
    json_array_foreach(jsonDumpers, i, jsonDumper) {
      char *nameIn = nullptr;
      char *pathIn = nullptr;

      ret = json_unpack_ex(jsonDumper, &err, 0, "{ s: s, s: s }", "name",
                           &nameIn, "path", &pathIn);
      if (ret)
        throw ConfigError(json, err, "node-config-node-smu",
                          "Failed to parse dumper configuration");

      int idx = signals->getIndexByName(nameIn);
      if (idx == -1)
        throw ConfigError(json, err, "node-config-node-smu",
                          "Could not find signal {} for dumper", nameIn);

      dumpers[idx].setPath(pathIn);
      dumpers[idx].openSocket();
      dumpers[idx].setActive();
    }
  }

  if (!modeIn || strcmp(modeIn, "MODE_FREERUN") == 0)
    mode = MODE_FREERUN;
  else if (strcmp(modeIn, "MODE_ONESHOT") == 0)
    mode = MODE_ONESHOT;
  else if (strcmp(modeIn, "MODE_VIRTUAL") == 0)
    mode = MODE_VIRTUAL;
  else if (strcmp(modeIn, "MODE_MAX") == 0)
    mode = MODE_MAX;

  if (!syncIn || strcmp(syncIn, "SYNC_PPS") == 0)
    sync = SYNC_PPS;
  else if (strcmp(syncIn, "SYNC_NONE") == 0)
    sync = SYNC_NONE;
  else if (strcmp(syncIn, "SYNC_NTP") == 0)
    sync = SYNC_NTP;
  else if (strcmp(syncIn, "SYNC_MAX") == 0)
    sync = SYNC_MAX;

  if (!fpsIn || strcmp(fpsIn, "FB_10FPS") == 0)
    fps = FB_10FPS;
  else if (strcmp(fpsIn, "FB_20FPS") == 0)
    fps = FB_20FPS;
  else if (strcmp(fpsIn, "FB_50FPS") == 0)
    fps = FB_50FPS;
  else if (strcmp(fpsIn, "FB_100FPS") == 0)
    fps = FB_100FPS;
  else if (strcmp(fpsIn, "FB_200FPS") == 0)
    fps = FB_200FPS;

  if (!sample_rateIn || strcmp(sample_rateIn, "FS_10kSPS") == 0)
    sample_rate = FS_10kSPS;
  else if (strcmp(sample_rateIn, "FS_1kSPS") == 0)
    sample_rate = FS_1kSPS;
  else if (strcmp(sample_rateIn, "FS_2kSPS") == 0)
    sample_rate = FS_2kSPS;
  else if (strcmp(sample_rateIn, "FS_5kSPS") == 0)
    sample_rate = FS_5kSPS;
  else if (strcmp(sample_rateIn, "FS_20kSPS") == 0)
    sample_rate = FS_20kSPS;
  else if (strcmp(sample_rateIn, "FS_25kSPS") == 0)
    sample_rate = FS_25kSPS;
  else if (strcmp(sample_rateIn, "FS_50kSPS") == 0)
    sample_rate = FS_50kSPS;
  else if (strcmp(sample_rateIn, "FS_100kSPS") == 0)
    sample_rate = FS_100kSPS;
  else if (strcmp(sample_rateIn, "FS_200kSPS") == 0) {
    sample_rate = FS_200kSPS;
  }

  if (json_is_object(in_json) && json_object_get(in_json, "vectorize")) {
    throw ConfigError(json, "node-config-node-smu",
                      "Vectorize cannot be overwritten for this node type!");
  }
  in.vectorize = sample_rate * 1000 / fps;

  return 0;
}

int SMUNode::start() {
  assert(state == State::PREPARED || state == State::PAUSED);

  int ret = Node::start();

  daq_cfg.rate = sample_rate;
  daq_cfg.buff = fps;
  daq_cfg.mode = mode;
  daq_cfg.sync = sync;

  fd = ::open(SMU_DEV, O_RDWR);
  if (fd < 0)
    logger->warn("Fail to open the device driver");

  // Associate kernel-to-user Task ID
  if (ioctl(fd, SMU_IOC_REG_TASK, 0)) {
    logger->warn("Fail to register the driver");
    ::close(fd);
  }

  // Map kernel-to-user shared memory
  void *sh_mem =
      mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  shared_mem = (uint8_t *)sh_mem;
  if (shared_mem == MAP_FAILED)
    logger->warn("Fail to map driver memory");

  // Install PPS signal handler
  sigemptyset(&act.sa_mask);
  act.sa_flags = (SA_SIGINFO | SA_RESTART);
  act.sa_sigaction = sync_signal;
  if (sigaction(SMU_SIG_SYNC, &act, nullptr))
    logger->warn("Fail to install PPS handler");

  // Install ADC signal handler
  sigemptyset(&act.sa_mask);
  act.sa_flags = (SA_SIGINFO | SA_RESTART);
  act.sa_sigaction = data_available_signal;
  if (sigaction(SMU_SIG_DATA, &act, nullptr))
    logger->warn("Fail to install ADC handler");

  // Update DAQ memory configuration
  shared_mem_pos = 0;
  shared_mem_inc = sizeof(smu_mcsc_t);
  shared_mem_dim = daq_cfg.rate;

  // Stop DAQ driver
  if (ioctl(fd, SMU_IOC_STOP, 0))
    logger->warn("Fail to stop the driver");

  // Configure DAQ driver
  if (::write(fd, &daq_cfg, sizeof(daq_cfg.rate)))
    logger->warn("Fail to configure the driver");

  // Start DAQ driver
  if (ioctl(fd, SMU_IOC_START, 0)) {
    logger->warn("Fail to start the driver");
  }

  if (!ret)
    state = State::STARTED;

  return ret;
}

int SMUNode::stop() {
  // Stop DAQ driver
  if (!ioctl(fd, SMU_IOC_STOP, 0)) {
    // Unmap kernel-to-user shared memory
    munmap(shared_mem, MMAP_SIZE);
    while (close(fd) != 0) {
    }
  }

  return 1;
}

void SMUNode::sync_signal(int, siginfo_t *info, void *) {

  ioctl(fd, SMU_IOC_GET_TIME, &sync_signal_mem_pos);
  sample_time.tv_nsec = sync_signal_mem_pos.tv_nsec; //macht nix
  sample_time.tv_sec = sync_signal_mem_pos.tv_sec;

  if (sample_time.tv_nsec > 500000000)
    sample_time.tv_sec += 1;
}

void SMUNode::data_available_signal(int, siginfo_t *info, void *) {

  mem_pos = (info->si_value.sival_int);
  pthread_cond_signal(&cv);
}

int SMUNode::_read(struct Sample *smps[], unsigned cnt) {
  struct timespec ts;
  ts.tv_sec = sample_time.tv_sec;
  pthread_mutex_lock(&mutex);

  pthread_cond_wait(&cv, &mutex);
  size_t mem_pos_local = mem_pos;

  smu_mcsc_t *p = (smu_mcsc_t *)shared_mem;

  for (unsigned j = 0; j < cnt; j++) {
    struct Sample *t = smps[j];
    ts.tv_nsec = mem_pos_local * 1e6 / (sample_rate);

    for (unsigned i = 0; i < 8; i++) {
      int16_t data = p[mem_pos_local].ch[i];
      t->data[i].f = ((double)data);
      if (dumpers[i].isActive())
        dumpers[i].writeDataBinary(1, &(t->data[i].f));
    }

    mem_pos_local++;

    t->flags = (int)SampleFlags::HAS_TS_ORIGIN | (int)SampleFlags::HAS_DATA |
               (int)SampleFlags::HAS_SEQUENCE;
    t->ts.origin = ts;
    t->sequence = sequence++;
    t->length = 8;
    t->signals = in.signals;
  }

  pthread_mutex_unlock(&mutex);

  return cnt;
}

static char n[] = "smu";
static char d[] = "SMU data acquisition";
static NodePlugin<SMUNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_POLL>
    p;
