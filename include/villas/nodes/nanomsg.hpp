/* Node type: nanomsg
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/list.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

// The maximum length of a packet which contains stuct msg.
#define NANOMSG_MAX_PACKET_LEN 1500

struct nanomsg {
	struct {
		int socket;
		struct List endpoints;
	} in, out;

	Format *formatter;
};

char * nanomsg_print(NodeCompat *n);

int nanomsg_init(NodeCompat *n);

int nanomsg_destroy(NodeCompat *n);

int nanomsg_parse(NodeCompat *n, json_t *json);

int nanomsg_start(NodeCompat *n);

int nanomsg_stop(NodeCompat *n);

int nanomsg_type_stop();

int nanomsg_reverse(NodeCompat *n);

int nanomsg_poll_fds(NodeCompat *n, int fds[]);

int nanomsg_netem_fds(NodeCompat *n, int fds[]);

int nanomsg_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int nanomsg_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} // namespace node
} // namespace villas
