/** Node type: infiniband
 *
 * @file
 * @author Dennis Potter <dennis@dennispotter.eu>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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
 * @addtogroup infiniband infiniband node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/pool.h>
#include <villas/io.h>
#include <villas/queue_signalled.h>
#include <rdma/rdma_cma.h>

/* Function pointer typedefs */
typedef void  (*ib_on_completion) (struct node*, struct ibv_wc*, int*);
typedef void * (*ib_poll_function) (void*);
typedef void * (*ib_event_function) (void*);

/* Enums */
enum poll_mode_e {
	EVENT,
	BUSY
};

struct r_addr_key_s {
	uint64_t remote_addr;
	uint32_t rkey;
};

struct infiniband {
	/* IBV/RDMA CM structs */
	struct context_s {
		struct rdma_cm_id *listen_id;
		struct rdma_cm_id *id;
		struct rdma_event_channel *ec;

		struct ibv_pd *pd;
		struct ibv_cq *recv_cq;
		struct ibv_cq *send_cq;
		struct ibv_comp_channel *comp_channel;
	} ctx;

	/* Work Completion related */
	struct poll_s {
		enum poll_mode_e poll_mode;

		/* On completion function */
		ib_on_completion on_compl;

		/* Busy poll or Event function */
		ib_poll_function poll_func;

		/* Poll thread */
		pthread_t cq_poller_thread;
	} poll;

	int stopThreads;

	/* Connection specific variables */
	struct connection_s {
		struct addrinfo *src_addr;
		struct addrinfo *dst_addr;
		enum rdma_port_space port_space;
		int timeout;

		struct r_addr_key_s *r_addr_key;

		pthread_t rdma_cm_event_thread;

		int available_recv_wrs;
	} conn;

	/* Memory related variables */
	struct ib_memory {
		struct pool p_recv;
		struct pool p_send;

		struct ibv_mr *mr_recv;
		struct ibv_mr *mr_send;
	} mem;

	/* Queue Pair init variables */
	struct ibv_qp_init_attr qp_init;

	/* Misc settings */
	int is_source;
	int cq_size;
};

/** @see node_type::reverse */
int infiniband_reverse(struct node *n);

/** @see node_type::print */
char * infiniband_print(struct node *n);

/** @see node_type::parse */
int infiniband_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int infiniband_start(struct node *n);

/** @see node_type::destroy */
int infiniband_destroy(struct node *n);

/** @see node_type::close */
int infiniband_stop(struct node *n);

/** @see node_type::init */
int infiniband_init(struct super_node *n);

/** @see node_type::deinit */
int infiniband_deinit();

/** @see node_type::read */
int infiniband_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int infiniband_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
