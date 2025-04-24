/* Node-type for SMU data acquisition.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>
#include <villas/dumper.hpp>
#include <villas/node.hpp>
#include <villas/queue_signalled.h>

extern "C" {

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
}

#define SMU_NCH 8

namespace villas {
namespace node {

typedef enum : unsigned char // DAQ sampling rate (kHz / kSPS)
{ FS_1kSPS = 1,
  FS_2kSPS = 2,
  FS_5kSPS = 5,
  FS_10kSPS = 10,
  FS_20kSPS = 20,
  FS_25kSPS = 25,
  FS_50kSPS = 50,
  FS_100kSPS = 100,
  FS_200kSPS = 200 } daq_rate_t;

typedef enum : unsigned char // DAQ buffer frame rate (Hz / FPS)
{ FB_10FPS = 10,
  FB_20FPS = 20,
  FB_50FPS = 50,
  FB_100FPS = 100,
  FB_200FPS = 200 } daq_buff_t;

typedef enum : unsigned char // DAQ acquisition mode
{ MODE_ONESHOT,              /*!< DAQ runs for 1 second */
  MODE_FREERUN,              /*!< DAQ runs continuously */
  MODE_VIRTUAL,              /*!< DAQ runs virtually */
  MODE_MAX } daq_mode_t;

typedef enum : unsigned char // DAQ synchronization mode
{ SYNC_NONE,                 /*!< Time base synchronization disabled */
  SYNC_PPS, /*!< Time base synchronization from GPS via pulse-per-second */
  SYNC_NTP, /*!< Time base synchronization from Server via network-time-protocol */
  SYNC_MAX } daq_sync_t;

typedef struct // DAQ parameters
{
  daq_rate_t rate; /*!< DAQ sampling rate */
  daq_buff_t buff; /*!< DAQ buffer frame rate */
  daq_mode_t mode; /*!< DAQ acquisition mode */
  daq_sync_t sync; /*!< DAQ synchronization mode */
} __attribute__((__packed__)) smu_daq_t;

typedef enum : unsigned short int // DSP reporting rate (Hz / PPS)
{ FR_EVENT = 0,
  FR_1PPS = 1,
  FR_2PPS = 2,
  FR_5PPS = 5,
  FR_10PPS = 10,
  FR_20PPS = 20,
  FR_25PPS = 25,
  FR_50PPS = 50,
  FR_100PPS = 100 } dsp_rate_t;

typedef struct // SMU multi-channel sample code
{
  int16_t ch[SMU_NCH];
} __attribute__((__packed__)) smu_mcsc_t;

typedef struct // SMU multi-channel sample real
{
  float ch[SMU_NCH];
} __attribute__((__packed__)) smu_mcsr_t;

typedef struct // SMU multi-channel conversion coefficients
{
  float k[SMU_NCH];
} __attribute__((__packed__)) smu_mcsk_t;

typedef struct {
  int64_t tv_sec;
  long tv_nsec;
} __attribute__((__packed__)) timespec64_t;

// IOCTL commands to communicate from the userspace to the kernel module
#define SMU_DEV "/dev/smu"
#define SMU_IOC_MAGIC 'k'
#define SMU_IOC_REG_TASK _IO(SMU_IOC_MAGIC, 0)
#define SMU_IOC_RESET _IO(SMU_IOC_MAGIC, 1)
#define SMU_IOC_START _IO(SMU_IOC_MAGIC, 2)
#define SMU_IOC_STOP _IO(SMU_IOC_MAGIC, 3)
#define SMU_IOC_GET_TIME _IOR(SMU_IOC_MAGIC, 4, timespec64_t *)
#define SMU_IOC_SET_CONF _IOW(SMU_IOC_MAGIC, 5, smu_conf_t *)
#define SMU_IOC_GET_CONF _IOR(SMU_IOC_MAGIC, 6, smu_conf_t *)

// Signal definitions
#define SMU_SIG_SYNC 41
#define SMU_SIG_DATA 42

#define PAGE_NUM 800
#define PAGE_SIZE 4096
#define MMAP_SIZE (PAGE_SIZE * PAGE_NUM)

class SMUNode : public Node {

private:
  daq_mode_t mode;
  daq_rate_t sample_rate;
  daq_buff_t fps;
  daq_sync_t sync;
  smu_daq_t daq_cfg;
  Dumper dumpers[SMU_NCH];
  static inline int fd;
  uint8_t *shared_mem;
  uint32_t shared_mem_pos;
  uint32_t shared_mem_inc;
  uint32_t shared_mem_dim;
  struct sigaction act;

  size_t buffer_size;
  static inline std::atomic<size_t> mem_pos = 0;
  static inline pthread_mutex_t mutex;
  static inline pthread_cond_t cv;
  static inline timespec64_t sync_signal_mem_pos = {0};
  static inline timespec sample_time = {0};
  static inline int smp_cnt = 0;

protected:
  virtual int _read(struct Sample *smps[], unsigned cnt);

public:
  SMUNode(const uuid_t &id = {}, const std::string &name = "");
  static void data_available_signal(int, siginfo_t *info, void *);
  static void sync_signal(int, siginfo_t *info, void *);

  virtual int parse(json_t *json) override;

  virtual int prepare();

  virtual int start();

  virtual int stop();
};

} /* namespace node */
} /* namespace villas */
