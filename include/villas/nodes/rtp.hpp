/** Node type: rtp
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <pthread.h>

#include <fstream>

#include <villas/list.hpp>
#include <villas/log.hpp>
#include <villas/format.hpp>
#include <villas/queue_signalled.h>
#include <villas/hooks/limit_rate.hpp>
#include <villas/hooks/decimate.hpp>
#include <villas/dsp/pid.hpp>

extern "C" {
	#include <re/re_sa.h>
	#include <re/re_rtp.h>
}

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;
class SuperNode;

/** The maximum length of a packet which contains rtp data. */
#define RTP_INITIAL_BUFFER_LEN 1500
#define RTP_PACKET_TYPE 21

enum class RTPHookType {
	DISABLED,
	DECIMATE,
	LIMIT_RATE
};

struct rtp {
	struct rtp_sock *rs;	/**< RTP socket */

	struct {
		struct sa saddr_rtp;	/**< Local/Remote address of the RTP socket */
		struct sa saddr_rtcp;	/**< Local/Remote address of the RTCP socket */
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

		/* PID parameters for rate controller */
		double Kp, Ki, Kd;
		double rate_min;

		double rate;
		double rate_source;		/**< Sample rate of source */

		std::ofstream *log;
		char *log_filename;
	} aimd;				/** AIMD state */

	struct CQueueSignalled recv_queue;
	struct mbuf *send_mb;
};

char * rtp_print(NodeCompat *n);

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

int rtp_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int rtp_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
