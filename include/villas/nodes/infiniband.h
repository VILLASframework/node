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

/* Forward declarations */
struct format_type;

enum poll_mode_e 
{
    EVENT,
    BUSY
};

struct infiniband {
    struct rdma_cm_id *id;
    struct rdma_event_channel *ec;

    struct context_s {
        struct ibv_pd *pd;
        struct ibv_cq *cq;
        struct ibv_comp_channel *comp_channel;
    } ctx;

    struct poll_s {
        enum poll_mode_e poll_mode;
        pthread_t cq_poller_thread;
    } poll;

    struct connection_s {
        struct addrinfo *src_addr;
        struct addrinfo *dst_addr;
        int timeout;
        enum rdma_port_space port_space;

        struct ibv_mr *mr_payload;
        struct r_addr_key_s *r_addr_key;
    } conn;

    struct ibv_qp_init_attr qp_init;

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
