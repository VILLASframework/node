/** Node type: rtp
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup rtp rtp node type
 * @ingroup node
 * @{
 */

#pragma once

#include <pthread.h>

#include <fstream>

extern "C" {
#include <re/re_sa.h>
}

#include <villas/node.h>
#include <villas/list.h>
#include <villas/log.hpp>
#include <villas/io.h>
#include <villas/queue_signalled.h>
#include <villas/hooks/limit_rate.hpp>
#include <villas/hooks/decimate.hpp>

/* Forward declarations */
struct format_type;

/** The maximum length of a packet which contains rtp data. */
#define RTP_INITIAL_BUFFER_LEN 1500
#define RTP_PACKET_TYPE 21

enum rtp_throttle_mode {
	RTCP_THROTTLE_DISABLED,
	RTCP_THROTTLE_HOOK_DECIMATE,
	RTCP_THROTTLE_HOOK_LIMIT_RATE
};

enum rtp_rtcp_mode {
	RTCP_MODE_AIMD
};

struct rtp {
	struct rtp_sock *rs;	/**< RTP socket */

	villas::Logger logger;

	struct {
		struct sa saddr_rtp;	/**< Local/Remote address of the RTP socket */
		struct sa saddr_rtcp;	/**< Local/Remote address of the RTCP socket */
	} in, out;

	struct format_type *format;
	struct io io;

	double rate;			/**< Sample rate of source */

	struct {
		int enabled;

		int num_rrs;

		enum rtp_rtcp_mode mode;

		enum rtp_throttle_mode throttle_mode;

		union {
			villas::node::DecimateHook *decimate;
			villas::node::LimitRateHook *limit_rate;
		} throttle_hook;
	} rtcp;

	struct {
		double a;
		double b;

		double last_rate;

		std::ofstream *log;
		char *log_filename;
	} aimd;				/** AIMD state */

	struct queue_signalled recv_queue;
	struct mbuf *send_mb;
};

extern "C" {

/** @see node_type::print */
char * rtp_print(struct node *n);

/** @see node_type::parse */
int rtp_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int rtp_start(struct node *n);

/** @see node_type::close */
int rtp_stop(struct node *n);

/** @see node_type::read */
int rtp_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int rtp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

}

/** @} */
