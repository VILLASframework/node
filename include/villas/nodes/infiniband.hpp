/* Node type: infiniband.
 *
 * Author: Dennis Potter <dennis@dennispotter.eu>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <rdma/rdma_cma.h>

#include <villas/format.hpp>
#include <villas/pool.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

// Constants
#define META_SIZE 24
#define GRH_SIZE 40
#define META_GRH_SIZE META_SIZE + GRH_SIZE
#define CHK_PER_ITER 2048

struct infiniband {
  // IBV/RDMA CM structs
  struct context_s {
    struct rdma_cm_id *listen_id;
    struct rdma_cm_id *id;
    struct rdma_event_channel *ec;

    struct ibv_pd *pd;
    struct ibv_cq *recv_cq;
    struct ibv_cq *send_cq;
    struct ibv_comp_channel *comp_channel;
  } ctx;

  // Queue Pair init variables
  struct ibv_qp_init_attr qp_init;

  // Size of receive and send completion queue
  int recv_cq_size;
  int send_cq_size;

  // Bool, set if threads should be aborted
  int stopThreads;

  // When most messages are sent inline, once every <X> cycles a signal must be sent.
  unsigned signaling_counter;
  unsigned periodic_signaling;

  // Connection specific variables
  struct connection_s {
    struct addrinfo *src_addr;
    struct addrinfo *dst_addr;

    // RDMA_PS_TCP or RDMA_PS_UDP
    enum rdma_port_space port_space;

    // Timeout for rdma_resolve_route
    int timeout;

    // Thread to monitor RDMA CM Event threads
    pthread_t rdma_cm_event_thread;

    // Bool, should data be send inline if possible?
    int send_inline;

    // Bool, should node have a fallback if it can't connect to a remote host?
    int use_fallback;

    // Counter to keep track of available recv. WRs
    unsigned available_recv_wrs;

    /* Fixed number to substract from min. number available
		 * WRs in receive queue */
    unsigned buffer_subtraction;

    // Unrealiable connectionless data
    struct ud_s {
      struct rdma_ud_param ud;
      struct ibv_ah *ah;
      void *grh_ptr;
      struct ibv_mr *grh_mr;
    } ud;

  } conn;

  // Misc settings
  int is_source;
};

int ib_reverse(NodeCompat *n);

char *ib_print(NodeCompat *n);

int ib_parse(NodeCompat *n, json_t *json);

int ib_check(NodeCompat *n);

int ib_start(NodeCompat *n);

int ib_destroy(NodeCompat *n);

int ib_stop(NodeCompat *n);

int ib_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int ib_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
