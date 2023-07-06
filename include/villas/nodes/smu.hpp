/** Node-type for loopback connections.
 *
 * @file
 * @author Manuel Pitz <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <jansson.h>
#include <villas/node.hpp>
#include <villas/queue_signalled.h>
#include <villas/dumper.hpp>


extern "C" {

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
}


#define SMU_NCH 8

namespace villas {
namespace node {

//static const smu_dsp_t dsp_cfg_default = {FR_1PPS, {"1","1","1","1","1","1","1","1"}, {"V1", "V2", "V3", "V4", "I1", "I2", "I3", "I4"}};
//const smu_dsp_t dsp_cfg_default = {FR_10PPS, {"1","1","1","1","1","1","1","1"}, {"V1", "V2", "V3", "V4", "I1", "I2", "I3", "I4"}, "dsp_raw"};
//const smu_net_t net_cfg_default = {"id", "", 7080, IPv4, TCP, "JSON", "", 7090, IPv4, TCP, "net_raw", ""};

/// ---------- SMU: DAQ settings section ----------

/**
 * @brief DAQ sampling rate (kHz / kSPS)
 *
 */
typedef enum : unsigned char
{
    FS_1kSPS   =   1,
    FS_2kSPS   =   2,
    FS_5kSPS   =   5,
    FS_10kSPS  =  10,
    FS_20kSPS  =  20,
    FS_25kSPS  =  25,
    FS_50kSPS  =  50,
    FS_100kSPS = 100,
    FS_200kSPS = 200
} daq_rate_t;

/**
 * @brief DAQ buffer frame rate (Hz / FPS)
 *
 */
typedef enum : unsigned char
{
    FB_10FPS  =  10,
    FB_20FPS  =  20,
    FB_50FPS  =  50,
    FB_100FPS = 100,
    FB_200FPS = 200
} daq_buff_t;

/**
 * @brief DAQ acquisition mode
 *
 */
typedef enum : unsigned char
{
    MODE_ONESHOT,                   /*!< DAQ runs for 1 second */
    MODE_FREERUN,                   /*!< DAQ runs continuously */
    MODE_VIRTUAL,                   /*!< DAQ runs virtually */
    MODE_MAX
} daq_mode_t;

/**
 * @brief DAQ synchronization mode
 *
 */
typedef enum : unsigned char
{
    SYNC_NONE,                      /*!< Time base synchronization disabled */
    SYNC_PPS,                       /*!< Time base synchronization from GPS via pulse-per-second */
    SYNC_NTP,                       /*!< Time base synchronization from Server via network-time-protocol */
    SYNC_MAX
} daq_sync_t;

/**
 * @brief DAQ parameters
 *
 */
typedef struct
{
    daq_rate_t rate;                /*!< DAQ sampling rate */
    daq_buff_t buff;                /*!< DAQ buffer frame rate */
    daq_mode_t mode;                /*!< DAQ acquisition mode */
    daq_sync_t sync;                /*!< DAQ synchronization mode */
} __attribute__((__packed__)) smu_daq_t;

/// ---------- SMU: DSP settings section ----------

/**
 * @brief DSP reporting rate (Hz / PPS)
 *
 */
typedef enum : unsigned short int
{
    FR_EVENT  = 0,
    FR_1PPS   = 1,
    FR_2PPS   = 2,
    FR_5PPS   = 5,
    FR_10PPS  = 10,
    FR_20PPS  = 20,
    FR_25PPS  = 25,
    FR_50PPS  = 50,
    FR_100PPS = 100
} dsp_rate_t;


/**
 * @brief SMU multi-channel sample code
 *
 */
typedef struct {
    int16_t ch[SMU_NCH];
} __attribute__((__packed__)) smu_mcsc_t;

/**
 * @brief SMU multi-channel sample real
 *
 */
typedef struct {
    float ch[SMU_NCH];
} __attribute__((__packed__)) smu_mcsr_t;

/**
 * @brief SMU multi-channel conversion coefficients
 *
 */
typedef struct {
    float k[SMU_NCH];
} __attribute__((__packed__)) smu_mcsk_t;


/**
 * @brief  IOCTL definitions
 *
 * Here are defined the different IOCTL commands used
 * to communicate from the userspace to the kernel module.
 */
#define SMU_DEV             "/dev/smu"
#define SMU_IOC_MAGIC       'k'
#define SMU_IOC_REG_TASK    _IO (SMU_IOC_MAGIC, 0)
#define SMU_IOC_RESET       _IO (SMU_IOC_MAGIC, 1)
#define SMU_IOC_START       _IO (SMU_IOC_MAGIC, 2)
#define SMU_IOC_STOP        _IO (SMU_IOC_MAGIC, 3)
#define SMU_IOC_SET_CONF    _IOW(SMU_IOC_MAGIC, 4, smu_conf_t*)
#define SMU_IOC_GET_CONF    _IOR(SMU_IOC_MAGIC, 5, smu_conf_t*)

/**
 * @brief  Signal definitions
 *
 * Here are defined the different values used to send
 * signals to the userspace
 */
#define SMU_SIG_SYNC        41
#define SMU_SIG_DATA        42

#define PAGE_NUM    800
#define PAGE_SIZE   4096
#define MMAP_SIZE   (PAGE_SIZE*PAGE_NUM)

class SMUNode : public Node {

private:
    daq_mode_t mode;
    daq_rate_t sample_rate;
    daq_buff_t fps;
    daq_sync_t sync;
    smu_daq_t daq_cfg;
    Dumper dumpers[SMU_NCH];
    int fd;
    uint8_t* shared_mem;
    uint32_t shared_mem_pos;
    uint32_t shared_mem_inc;
    uint32_t shared_mem_dim;
    struct sigaction act;


    size_t buffer_size;
    static size_t mem_pos;
    static pthread_mutex_t mutex;
    static pthread_cond_t cv;

protected:
	virtual
	int _read(struct Sample * smps[], unsigned cnt);

public:
	SMUNode(const std::string &name = "");
    static void data_available_signal(int, siginfo_t *info, void*);
    static void sync_signal(int, siginfo_t *info, void*);

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;

	virtual
	int prepare();

	virtual
	int start();

	virtual
	int stop();
};

} /* namespace node */
} /* namespace villas */
