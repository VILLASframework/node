/* Node type: rtp.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Marvin Klimke <marvin.klimke@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <pthread.h>

#include <fstream>

#include <villas/dsp/pid.hpp>
#include <villas/format.hpp>
#include <villas/hooks/decimate.hpp>
#include <villas/hooks/limit_rate.hpp>
#include <villas/list.hpp>
#include <villas/log.hpp>
#include <villas/queue_signalled.h>

extern "C" {
#include <re/re_sa.h>
#include <re/re_rtp.h>
}

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;
class SuperNode;

// The maximum length of a packet which contains rtp data.
#define RTP_INITIAL_BUFFER_LEN 1500
#define RTP_PACKET_TYPE 21

enum class RTPHookType { DISABLED, DECIMATE, LIMIT_RATE };

struct rtp {
  struct rtp_sock *rs; // RTP socket

  struct {
    struct sa saddr_rtp;  // Local/Remote address of the RTP socket
    struct sa saddr_rtcp; // Local/Remote address of the RTCP socket
  } in, out;

  Format *formatter;

  struct {
    int enabled;

    int num_rrs;
  } rtcp;

  struct {
    double a;
    double b;

    enum RTPHookType rate_hook_type;

    LimitHook::Ptr rate_hook;
    dsp::PID rate_pid;

    // PID parameters for rate controller
    double Kp, Ki, Kd;
    double rate_min;

    double rate;
    double rate_source; // Sample rate of source

    std::ofstream *log;
    char *log_filename;
  } aimd; // AIMD state

  struct CQueueSignalled recv_queue;
  struct mbuf *send_mb;
};

char *rtp_print(NodeCompat *n);

int rtp_parse(NodeCompat *n, json_t *json);

int rtp_init(NodeCompat *n);

int rtp_destroy(NodeCompat *n);

int rtp_reverse(NodeCompat *n);

int rtp_type_start(SuperNode *sn);

int rtp_type_stop();

int rtp_start(NodeCompat *n);

int rtp_stop(NodeCompat *n);

int rtp_netem_fds(NodeCompat *n, int fds[]);

int rtp_poll_fds(NodeCompat *n, int fds[]);

int rtp_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int rtp_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
