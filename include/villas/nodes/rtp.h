/** Node type: rtp
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <re/re_sa.h>

#include <villas/node.h>
#include <villas/list.h>
#include <villas/io.h>
#include <villas/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The maximum length of a packet which contains stuct rtp. */
#define RTP_INITIAL_BUFFER_LEN 1500

/* Forward declarations */
struct format_type;

struct rtp {
	struct rtp_sock *rs;	/**< RTP socket */

	struct sa local_rtp;	/**< Local address of the RTP socket */
	struct sa local_rtcp;	/**< Local address of the RTCP socket */
	struct sa remote_rtp;	/**< Remote address of the RTP socket */
	struct sa remote_rtcp;	/**< Remote address of the RTCP socket */

	struct format_type *format;
	struct io io;

	bool enable_rtcp;

	struct queue recv_queue;
};

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

#ifdef __cplusplus
}
#endif

/** @} */
