/* Node type: infiniband.
 *
 * Author: Dennis Potter <dennis@dennispotter.eu>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <netdb.h>

#include <villas/exceptions.hpp>
#include <villas/memory/ib.h>
#include <villas/node/config.hpp>
#include <villas/node/memory.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/infiniband.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static int ib_disconnect(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  struct ibv_wc wc[MAX(ib->recv_cq_size, ib->send_cq_size)];
  int wcs;

  n->logger->debug("Starting to clean up");

  rdma_disconnect(ib->ctx.id);

  // If there is anything in the Completion Queue, it should be given back to the framework Receive Queue.
  while (ib->conn.available_recv_wrs) {
    wcs = ibv_poll_cq(ib->ctx.recv_cq, ib->recv_cq_size, wc);

    ib->conn.available_recv_wrs -= wcs;

    for (int j = 0; j < wcs; j++)
      sample_decref((struct Sample *)(intptr_t)(wc[j].wr_id));
  }

  // Send Queue
  while ((wcs = ibv_poll_cq(ib->ctx.send_cq, ib->send_cq_size, wc)))
    for (int j = 0; j < wcs; j++)
      if (wc[j].wr_id > 0)
        sample_decref((struct Sample *)(intptr_t)(wc[j].wr_id));

  // Destroy QP
  rdma_destroy_qp(ib->ctx.id);

  n->logger->debug("Destroyed QP");

  return ib->stopThreads;
}

static void ib_build_ibv(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  n->logger->debug("Starting to build IBV components");

  // Create completion queues (No completion channel!)
  ib->ctx.recv_cq =
      ibv_create_cq(ib->ctx.id->verbs, ib->recv_cq_size, nullptr, nullptr, 0);
  if (!ib->ctx.recv_cq)
    throw RuntimeError("Could not create receive completion queue");

  n->logger->debug("Created receive Completion Queue");

  ib->ctx.send_cq =
      ibv_create_cq(ib->ctx.id->verbs, ib->send_cq_size, nullptr, nullptr, 0);
  if (!ib->ctx.send_cq)
    throw RuntimeError("Could not create send completion queue");

  n->logger->debug("Created send Completion Queue");

  // Prepare remaining Queue Pair (QP) attributes
  ib->qp_init.send_cq = ib->ctx.send_cq;
  ib->qp_init.recv_cq = ib->ctx.recv_cq;

  // Create the actual QP
  ret = rdma_create_qp(ib->ctx.id, ib->ctx.pd, &ib->qp_init);
  if (ret)
    throw RuntimeError("Failed to create Queue Pair");

  n->logger->debug("Created Queue Pair with {} receive and {} send elements",
                   ib->qp_init.cap.max_recv_wr, ib->qp_init.cap.max_send_wr);

  if (ib->conn.send_inline)
    n->logger->info("Maximum inline size is set to {} byte",
                    ib->qp_init.cap.max_inline_data);
}

static int ib_addr_resolved(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  n->logger->debug("Successfully resolved address");

  // Build all components from IB Verbs
  ib_build_ibv(n);

  // Resolve address
  ret = rdma_resolve_route(ib->ctx.id, ib->conn.timeout);
  if (ret)
    throw RuntimeError("Failed to resolve route");

  return 0;
}

static int ib_route_resolved(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  struct rdma_conn_param cm_params;
  memset(&cm_params, 0, sizeof(cm_params));

  // Send connection request
  ret = rdma_connect(ib->ctx.id, &cm_params);
  if (ret)
    throw RuntimeError("Failed to connect");

  n->logger->debug("Called rdma_connect");

  return 0;
}

static int ib_connect_request(NodeCompat *n, struct rdma_cm_id *id) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  n->logger->debug("Received a connection request!");

  ib->ctx.id = id;
  ib_build_ibv(n);

  struct rdma_conn_param cm_params;
  memset(&cm_params, 0, sizeof(cm_params));

  // Accept connection request
  ret = rdma_accept(ib->ctx.id, &cm_params);
  if (ret)
    throw RuntimeError("Failed to connect");

  n->logger->info("Successfully accepted connection request");

  return 0;
}

int villas::node::ib_reverse(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();

  SWAP(ib->conn.src_addr, ib->conn.dst_addr);

  return 0;
}

int villas::node::ib_parse(NodeCompat *n, json_t *json) {
  auto *ib = n->getData<struct infiniband>();

  throw ConfigError(json, "The infiniband node-type is currently broken!");

  int ret;
  char *local = nullptr, *remote = nullptr, *lasts;
  const char *transport_mode = "RC";
  int timeout = 1000;
  int recv_cq_size = 128;
  int send_cq_size = 128;
  int max_send_wr = 128;
  int max_recv_wr = 128;
  int max_inline_data = 0;
  int send_inline = 1;
  int vectorize_in = 1;
  int vectorize_out = 1;
  int buffer_subtraction = 16;
  int use_fallback = 1;

  // Parse JSON files and copy to local variables
  json_t *json_in = nullptr;
  json_t *json_out = nullptr;
  json_error_t err;

  ret =
      json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s?: s }", "in", &json_in,
                     "out", &json_out, "rdma_transport_mode", &transport_mode);
  if (ret)
    throw ConfigError(json, err, "node-config-node-ib");

  if (json_in) {
    ret = json_unpack_ex(
        json_in, &err, 0, "{ s?: s, s?: i, s?: i, s?: i, s?: i}", "address",
        &local, "cq_size", &recv_cq_size, "max_wrs", &max_recv_wr, "vectorize",
        &vectorize_in, "buffer_subtraction", &buffer_subtraction);
    if (ret)
      throw ConfigError(json_in, err, "node-config-node-ib-in");
  }

  if (json_out) {
    ret = json_unpack_ex(
        json_out, &err, 0,
        "{ s?: s, s?: i, s?: i, s?: i, s?: i, s?: b, s?: i, s?: b, s?: i}",
        "address", &remote, "resolution_timeout", &timeout, "cq_size",
        &send_cq_size, "max_wrs", &max_send_wr, "max_inline_data",
        &max_inline_data, "send_inline", &send_inline, "vectorize",
        &vectorize_out, "use_fallback", &use_fallback, "periodic_signaling",
        &ib->periodic_signaling);
    if (ret)
      throw ConfigError(json_out, err, "node-config-node-ib-out");

    if (remote) {
      ib->is_source = 1;

      n->logger->debug("Setup as source and target");
    }
  } else {
    ib->is_source = 0;

    n->logger->debug("Setup as target");
  }

  // Set fallback mode
  ib->conn.use_fallback = use_fallback;

  // Set vectorize mode. Do not print, since framework will print this information
  n->in.vectorize = vectorize_in;
  n->out.vectorize = vectorize_out;

  // Set buffer subtraction
  ib->conn.buffer_subtraction = buffer_subtraction;

  n->logger->debug("Set buffer subtraction to {}", buffer_subtraction);

  // Translate IP:PORT to a struct addrinfo
  char *ip_adr = strtok_r(local, ":", &lasts);
  char *port = strtok_r(nullptr, ":", &lasts);

  ret = getaddrinfo(ip_adr, port, nullptr, &ib->conn.src_addr);
  if (ret)
    throw RuntimeError("Failed to resolve local address '{}': {}", local,
                       gai_strerror(ret));

  n->logger->debug("Translated {}:{} to a struct addrinfo", ip_adr, port);

  // Translate port space
  if (strcmp(transport_mode, "RC") == 0) {
    ib->conn.port_space = RDMA_PS_TCP;
    ib->qp_init.qp_type = IBV_QPT_RC;
  } else if (strcmp(transport_mode, "UC") == 0) {
#ifdef RDMA_CMA_H_CUSTOM
    ib->conn.port_space = RDMA_PS_IB;
    ib->qp_init.qp_type = IBV_QPT_UC;
#else
    throw RuntimeError("Unreliable Connected (UC) mode is only available with "
                       "an adapted version of librdma. "
                       "Please read the Infiniband node type Documentation for "
                       "more information on UC!");
#endif
  } else if (strcmp(transport_mode, "UD") == 0) {
    ib->conn.port_space = RDMA_PS_UDP;
    ib->qp_init.qp_type = IBV_QPT_UD;
  } else
    throw RuntimeError("Invalid transport_mode = '{}'!", transport_mode);

  n->logger->debug("Set transport mode to {}", transport_mode);

  // Set timeout
  ib->conn.timeout = timeout;

  n->logger->debug("Set timeout to {}", timeout);

  // Set completion queue size
  ib->recv_cq_size = recv_cq_size;
  ib->send_cq_size = send_cq_size;

  n->logger->debug("Set Completion Queue size to {} & {} (in & out)",
                   recv_cq_size, send_cq_size);

  // Translate inline mode
  ib->conn.send_inline = send_inline;

  n->logger->debug("Set send_inline to {}", send_inline);

  // Set max. send and receive Work Requests
  ib->qp_init.cap.max_send_wr = max_send_wr;
  ib->qp_init.cap.max_recv_wr = max_recv_wr;

  n->logger->debug("Set max_send_wr and max_recv_wr to {} and {}, respectively",
                   max_send_wr, max_recv_wr);

  // Set available receive Work Requests to 0
  ib->conn.available_recv_wrs = 0;

  // Set remaining QP attributes
  ib->qp_init.cap.max_send_sge = 4;
  ib->qp_init.cap.max_recv_sge = (ib->conn.port_space == RDMA_PS_UDP) ? 5 : 4;

  // Set number of bytes to be send inline
  ib->qp_init.cap.max_inline_data = max_inline_data;

  // If node will send data, set remote address
  if (ib->is_source) {
    // Translate address info
    char *ip_adr = strtok_r(remote, ":", &lasts);
    char *port = strtok_r(nullptr, ":", &lasts);

    ret = getaddrinfo(ip_adr, port, nullptr, &ib->conn.dst_addr);
    if (ret)
      throw RuntimeError("Failed to resolve remote address '{}': {}", remote,
                         gai_strerror(ret));

    n->logger->debug("Translated {}:{} to a struct addrinfo", ip_adr, port);
  }

  return 0;
}

int villas::node::ib_check(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();

  // Check if read substraction makes sense
  if (ib->conn.buffer_subtraction < 2 * n->in.vectorize)
    throw RuntimeError(
        "The buffer substraction value must be bigger than 2 * in.vectorize");

  if (ib->conn.buffer_subtraction >=
      ib->qp_init.cap.max_recv_wr - n->in.vectorize)
    throw RuntimeError("The buffer substraction value cannot be bigger than "
                       "in.max_wrs - in.vectorize");

  // Check if the set value is a power of 2, and warn the user if this is not the case
  unsigned max_send_pow = (int)pow(2, ceil(log2(ib->qp_init.cap.max_send_wr)));
  unsigned max_recv_pow = (int)pow(2, ceil(log2(ib->qp_init.cap.max_recv_wr)));

  if (ib->qp_init.cap.max_send_wr != max_send_pow) {
    n->logger->warn("Max nr. of send WRs ({}) is not a power of 2! It will be "
                    "changed to a power of 2: {}",
                    ib->qp_init.cap.max_send_wr, max_send_pow);

    // Change it now, because otherwise errors are possible in ib_start().
    ib->qp_init.cap.max_send_wr = max_send_pow;
  }

  if (ib->qp_init.cap.max_recv_wr != max_recv_pow) {
    n->logger->warn("Max nr. of recv WRs ({}) is not a power of 2! It will be "
                    "changed to a power of 2: {}",
                    ib->qp_init.cap.max_recv_wr, max_recv_pow);

    // Change it now, because otherwise errors are possible in ib_start().
    ib->qp_init.cap.max_recv_wr = max_recv_pow;
  }

  // Check maximum size of max_recv_wr and max_send_wr
  if (ib->qp_init.cap.max_send_wr > 8192)
    n->logger->warn("Max number of send WRs ({}) is bigger than send queue!",
                    ib->qp_init.cap.max_send_wr);

  if (ib->qp_init.cap.max_recv_wr > 8192)
    n->logger->warn("Max number of receive WRs ({}) is bigger than send queue!",
                    ib->qp_init.cap.max_recv_wr);

  /* Set periodic signaling
	 * This is done here, so that it uses the checked max_send_wr value */
  if (ib->periodic_signaling == 0)
    ib->periodic_signaling = ib->qp_init.cap.max_send_wr / 2;

  // Warn user if he changed the default inline value
  if (ib->qp_init.cap.max_inline_data != 0)
    n->logger->warn("You changed the default value of max_inline_data. This "
                    "might influence the maximum number "
                    "of outstanding Work Requests in the Queue Pair and can be "
                    "a reason for the Queue Pair creation to fail");

  return 0;
}

char *villas::node::ib_print(NodeCompat *n) { return 0; }

int villas::node::ib_destroy(NodeCompat *n) { return 0; }

static void ib_create_bind_id(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  /* Create rdma_cm_id
	 *
	 * The unreliable connected mode is officially not supported by the rdma_cm library. Only the Reliable
	 * Connected mode (RDMA_PS_TCP) and the Unreliable Datagram mode (RDMA_PS_UDP). Although it is not officially
	 * supported, it is possible to use it with a few small adaptions to the sourcecode. To enable the
	 * support for UC connections follow the steps below:
	 *
	 * 1. git clone https://github.com/linux-rdma/rdma-core
	 * 2. cd rdma-core
	 * 2. Edit librdmacm/cma.c and remove the keyword 'static' in front of:
	 *
	 *     static int rdma_create_id2(struct rdma_event_channel *channel,
	 *         struct rdma_cm_id **id, void *context,
	 *         enum rdma_port_space ps, enum ibv_qp_type qp_type)
	 *
	 * 3. Edit librdmacm/rdma_cma.h and add the following two entries to the file:
	 *
	 *     #define RDMA_CMA_H_CUSTOM
	 *
	 *     int rdma_create_id2(struct rdma_event_channel *channel,
	 *         struct rdma_cm_id **id, void *context,
	 *         enum rdma_port_space ps, enum ibv_qp_type qp_type);
	 *
	 * 4. Edit librdmacm/librdmacm.map and add a new line with:
	 *
	 *     rdma_create_id2
	 *
	 * 5. ./build.sh
	 * 6. cd build && sudo make install
	 *
	 */
#ifdef RDMA_CMA_H_CUSTOM
  ret = rdma_create_id2(ib->ctx.ec, &ib->ctx.id, nullptr, ib->conn.port_space,
                        ib->qp_init.qp_type);
#else
  ret = rdma_create_id(ib->ctx.ec, &ib->ctx.id, nullptr, ib->conn.port_space);
#endif
  if (ret)
    throw RuntimeError("Failed to create rdma_cm_id: {}", gai_strerror(ret));

  n->logger->debug("Created rdma_cm_id");

  // Bind rdma_cm_id to the HCA
  ret = rdma_bind_addr(ib->ctx.id, ib->conn.src_addr->ai_addr);
  if (ret)
    throw RuntimeError("Failed to bind to local device: {}", gai_strerror(ret));

  n->logger->debug("Bound rdma_cm_id to Infiniband device");

  /* The ID will be overwritten for the target. If the event type is
	 * RDMA_CM_EVENT_CONNECT_REQUEST, >then this references a new id for
	 * that communication.
	 */
  ib->ctx.listen_id = ib->ctx.id;
}

static void ib_continue_as_listen(NodeCompat *n, struct rdma_cm_event *event) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  if (ib->conn.use_fallback)
    n->logger->warn("Trying to continue as listening node");
  else
    throw RuntimeError("Cannot establish a connection with remote host! If you "
                       "want that {} tries to "
                       "continue as listening node in such cases, set "
                       "use_fallback = true in the configuration");

  n->setState(State::STARTED);

  // Acknowledge event
  rdma_ack_cm_event(event);

  // Destroy ID
  rdma_destroy_id(ib->ctx.listen_id);

  // Create rdma_cm_id and bind to device
  ib_create_bind_id(n);

  // Listen to id for events
  ret = rdma_listen(ib->ctx.listen_id, 10);
  if (ret)
    throw RuntimeError("Failed to listen to rdma_cm_id");

  // Node is not a source (and will not send data
  ib->is_source = 0;

  n->logger->info("Use listening mode");
}

static void *ib_rdma_cm_event_thread(void *ctx) {
  auto *n = (NodeCompat *)ctx;
  auto *ib = n->getData<struct infiniband>();
  struct rdma_cm_event *event;
  int ret = 0;

  n->logger->debug("Started rdma_cm_event thread");

  // Wait until node is completely started
  while (n->getState() != State::STARTED)
    ;

  // Monitor event channel
  while (rdma_get_cm_event(ib->ctx.ec, &event) == 0) {
    n->logger->debug("Received communication event: {}",
                     rdma_event_str(event->event));

    switch (event->event) {
    case RDMA_CM_EVENT_ADDR_RESOLVED:
      ret = ib_addr_resolved(n);
      break;

    case RDMA_CM_EVENT_ADDR_ERROR:
      n->logger->warn("Address resolution (rdma_resolve_addr) failed!");

      ib_continue_as_listen(n, event);

      break;

    case RDMA_CM_EVENT_ROUTE_RESOLVED:
      ret = ib_route_resolved(n);
      break;

    case RDMA_CM_EVENT_ROUTE_ERROR:
      n->logger->warn("Route resolution (rdma_resovle_route) failed!");

      ib_continue_as_listen(n, event);

      break;

    case RDMA_CM_EVENT_UNREACHABLE:
      n->logger->warn("Remote server unreachable!");

      ib_continue_as_listen(n, event);
      break;

    case RDMA_CM_EVENT_CONNECT_REQUEST:
      ret = ib_connect_request(n, event->id);

      /* A target UDP node will never really connect. In order to receive data,
				 * we set it to connected after it answered the connection request
				 * with rdma_connect.
				 */
      if (ib->conn.port_space == RDMA_PS_UDP && !ib->is_source)
        n->setState(State::CONNECTED);
      else
        n->setState(State::PENDING_CONNECT);

      break;

    case RDMA_CM_EVENT_CONNECT_ERROR:
      n->logger->warn(
          "An error has occurred trying to establish a connection!");

      ib_continue_as_listen(n, event);

      break;

    case RDMA_CM_EVENT_REJECTED:
      n->logger->warn("Connection request or response was rejected by the "
                      "remote end point!");

      ib_continue_as_listen(n, event);

      break;

    case RDMA_CM_EVENT_ESTABLISHED:
      // If the connection is unreliable connectionless, set appropriate variables
      if (ib->conn.port_space == RDMA_PS_UDP) {
        ib->conn.ud.ud = event->param.ud;
        ib->conn.ud.ah = ibv_create_ah(ib->ctx.pd, &ib->conn.ud.ud.ah_attr);
      }

      n->setState(State::CONNECTED);

      n->logger->info("Connection established");

      break;

    case RDMA_CM_EVENT_DISCONNECTED:
      n->setState(State::STARTED);

      ret = ib_disconnect(n);

      if (!ret)
        n->logger->info("Host disconnected. Ready to accept new connections.");

      break;

    case RDMA_CM_EVENT_TIMEWAIT_EXIT:
      break;

    default:
      throw RuntimeError("Unknown event occurred: {}", (int)event->event);
    }

    rdma_ack_cm_event(event);

    if (ret)
      break;
  }

  return nullptr;
}

int villas::node::ib_start(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  n->logger->debug("Started ib_start");

  // Create event channel
  ib->ctx.ec = rdma_create_event_channel();
  if (!ib->ctx.ec)
    throw RuntimeError("Failed to create event channel!");

  n->logger->debug("Created event channel");

  // Create rdma_cm_id and bind to device
  ib_create_bind_id(n);

  n->logger->debug("Initialized Work Completion Buffer");

  // Resolve address or listen to rdma_cm_id
  if (ib->is_source) {
    // Resolve address
    ret = rdma_resolve_addr(ib->ctx.id, nullptr, ib->conn.dst_addr->ai_addr,
                            ib->conn.timeout);
    if (ret)
      throw RuntimeError("Failed to resolve remote address after {}ms: {}",
                         ib->conn.timeout, gai_strerror(ret));
  } else {
    // Listen on rdma_cm_id for events
    ret = rdma_listen(ib->ctx.listen_id, 10);
    if (ret)
      throw RuntimeError("Failed to listen to rdma_cm_id");

    n->logger->debug("Started to listen to rdma_cm_id");
  }

  // Allocate protection domain
  ib->ctx.pd = ibv_alloc_pd(ib->ctx.id->verbs);
  if (!ib->ctx.pd)
    throw RuntimeError("Could not allocate protection domain");

  n->logger->debug("Allocated Protection Domain");

  // Allocate space for 40 Byte GHR. We don't use this.
  if (ib->conn.port_space == RDMA_PS_UDP) {
    ib->conn.ud.grh_ptr = new char[GRH_SIZE];
    if (!ib->conn.ud.grh_ptr)
      throw MemoryAllocationError();

    ib->conn.ud.grh_mr = ibv_reg_mr(ib->ctx.pd, ib->conn.ud.grh_ptr, GRH_SIZE,
                                    IBV_ACCESS_LOCAL_WRITE);
  }

  /* Several events should occur on the event channel, to make
	 * sure the nodes are succesfully connected.
	 */
  n->logger->debug("Starting to monitor events on rdma_cm_id");

  // Create thread to monitor rdma_cm_event channel
  ret = pthread_create(&ib->conn.rdma_cm_event_thread, nullptr,
                       ib_rdma_cm_event_thread, n);
  if (ret)
    throw RuntimeError("Failed to create thread to monitor rdma_cm events: {}",
                       gai_strerror(ret));

  return 0;
}

int villas::node::ib_stop(NodeCompat *n) {
  auto *ib = n->getData<struct infiniband>();
  int ret;

  n->logger->debug("Called ib_stop");

  ib->stopThreads = 1;

  /* Call RDMA disconnect function
	 * Will flush all outstanding WRs to the Completion Queue and
	 * will call RDMA_CM_EVENT_DISCONNECTED if that is done.
	 */
  if (n->getState() == State::CONNECTED && ib->conn.port_space != RDMA_PS_UDP) {
    ret = rdma_disconnect(ib->ctx.id);

    if (ret)
      throw RuntimeError("Error while calling rdma_disconnect: {}",
                         gai_strerror(ret));

    n->logger->debug("Called rdma_disconnect");
  } else {
    pthread_cancel(ib->conn.rdma_cm_event_thread);

    n->logger->debug(
        "Called pthread_cancel() on communication management thread.");
  }

  n->logger->info("Disconnecting... Waiting for threads to join.");

  // Wait for event thread to join
  ret = pthread_join(ib->conn.rdma_cm_event_thread, nullptr);
  if (ret)
    throw RuntimeError("Error while joining rdma_cm_event_thread: {}", ret);

  n->logger->debug("Joined rdma_cm_event_thread");

  // Destroy RDMA CM ID
  rdma_destroy_id(ib->ctx.id);
  n->logger->debug("Destroyed rdma_cm_id");

  // Dealloc Protection Domain
  ibv_dealloc_pd(ib->ctx.pd);
  n->logger->debug("Destroyed protection domain");

  // Destroy event channel
  rdma_destroy_event_channel(ib->ctx.ec);
  n->logger->debug("Destroyed event channel");

  n->logger->info("Successfully stopped node");

  return 0;
}

int villas::node::ib_read(NodeCompat *n, struct Sample *const smps[],
                          unsigned cnt) {
  auto *ib = n->getData<struct infiniband>();
  struct ibv_wc wc[cnt];
  struct ibv_recv_wr wr[cnt], *bad_wr = nullptr;
  struct ibv_sge sge[cnt][ib->qp_init.cap.max_recv_sge];
  struct ibv_mr *mr;
  struct timespec ts_receive;
  int ret = 0, wcs = 0, read_values = 0, max_wr_post;

  n->logger->debug("ib_read is called");

  if (n->getState() == State::CONNECTED ||
      n->getState() == State::PENDING_CONNECT) {

    max_wr_post = cnt;

    /* Poll Completion Queue
		 * If we've already posted enough receive WRs, try to pull cnt
		 */
    if (ib->conn.available_recv_wrs >=
        (ib->qp_init.cap.max_recv_wr - ib->conn.buffer_subtraction)) {
      for (int i = 0;; i++) {
        if (i % CHK_PER_ITER == CHK_PER_ITER - 1)
          pthread_testcancel();

        /* If IB node disconnects or if it is still in State::PENDING_CONNECT, ib_read
				 * should return immediately if this condition holds
				 */
        if (n->getState() != State::CONNECTED)
          return 0;

        wcs = ibv_poll_cq(ib->ctx.recv_cq, cnt, wc);
        if (wcs) {
          // Get time directly after something arrived in Completion Queue
          ts_receive = time_now();

          n->logger->debug("Received {} Work Completions", wcs);

          read_values = wcs; // Value to return
          max_wr_post = wcs; // Make space free in smps[]

          break;
        }
      }

      /* All samples (wcs * received + unposted) should be released. Let
			 * *release be equal to allocated.
			 *
			 * This is set in the framework, before this function was called.
			 */
    } else {
      ib->conn.available_recv_wrs += max_wr_post;

      // TODO: fix release logic
      // *release = 0; // While we fill the receive queue, we always use all samples
    }

    // Get Memory Region
    mr = memory::ib_get_mr(pool_buffer(sample_pool(smps[0])));

    for (int i = 0; i < max_wr_post; i++) {
      int j = 0;

      // Prepare receive Scatter/Gather element

      // First 40 byte of UD data are GRH and unused in our case
      if (ib->conn.port_space == RDMA_PS_UDP) {
        sge[i][j].addr = (uint64_t)ib->conn.ud.grh_ptr;
        sge[i][j].length = GRH_SIZE;
        sge[i][j].lkey = ib->conn.ud.grh_mr->lkey;

        j++;
      }

      // Sequence
      sge[i][j].addr = (uint64_t)&smps[i]->sequence;
      sge[i][j].length = sizeof(smps[i]->sequence);
      sge[i][j].lkey = mr->lkey;

      j++;

      // Timespec origin
      sge[i][j].addr = (uint64_t)&smps[i]->ts.origin;
      sge[i][j].length = sizeof(smps[i]->ts.origin);
      sge[i][j].lkey = mr->lkey;

      j++;

      sge[i][j].addr = (uint64_t)&smps[i]->data;
      sge[i][j].length = SAMPLE_DATA_LENGTH(DEFAULT_SAMPLE_LENGTH);
      sge[i][j].lkey = mr->lkey;

      j++;

      // Prepare a receive Work Request
      wr[i].wr_id = (uintptr_t)smps[i];
      wr[i].next = &wr[i + 1];
      wr[i].sg_list = sge[i];
      wr[i].num_sge = j;
    }

    wr[max_wr_post - 1].next = nullptr;

    n->logger->debug("Prepared {} new receive Work Requests", max_wr_post);
    n->logger->debug("{} receive Work Requests in Receive Queue",
                     ib->conn.available_recv_wrs);

    // Post list of Work Requests
    ret = ibv_post_recv(ib->ctx.id->qp, &wr[0], &bad_wr);
    if (ret)
      throw RuntimeError("Was unable to post receive WR: {}, bad WR ID: {:#x}",
                         ret, bad_wr->wr_id);

    n->logger->debug("Succesfully posted receive Work Requests");

    // Doesn't start if wcs == 0
    for (int j = 0; j < wcs; j++) {
      if (!((wc[j].opcode & IBV_WC_RECV) && wc[j].status == IBV_WC_SUCCESS)) {
        // Drop all values, we don't know where the error occured
        read_values = 0;
      }

      if (wc[j].status == IBV_WC_WR_FLUSH_ERR)
        n->logger->debug("Received IBV_WC_WR_FLUSH_ERR (ib_read). Ignore it.");
      else if (wc[j].status != IBV_WC_SUCCESS)
        n->logger->warn("Work Completion status was not IBV_WC_SUCCESS: {}",
                        (int)wc[j].status);

      /* 32 byte of meta data is always transferred. We should substract it.
			 * Furthermore, in case of an unreliable connection, a 40 byte
			 * global routing header is transferred. This should be substracted as well.
			 */
      int correction =
          (ib->conn.port_space == RDMA_PS_UDP) ? META_GRH_SIZE : META_SIZE;

      // TODO: fix release logic
      // smps[j] = (struct Sample *) (wc[j].wr_id);

      smps[j]->length = SAMPLE_NUMBER_OF_VALUES(wc[j].byte_len - correction);
      smps[j]->ts.received = ts_receive;
      smps[j]->flags = (int)SampleFlags::HAS_TS_ORIGIN |
                       (int)SampleFlags::HAS_TS_RECEIVED |
                       (int)SampleFlags::HAS_SEQUENCE;
      smps[j]->signals = n->getInputSignals(false);
    }
  }
  return read_values;
}

int villas::node::ib_write(NodeCompat *n, struct Sample *const smps[],
                           unsigned cnt) {
  auto *ib = n->getData<struct infiniband>();
  struct ibv_send_wr wr[cnt], *bad_wr = nullptr;
  struct ibv_sge sge[cnt][ib->qp_init.cap.max_recv_sge];
  struct ibv_wc wc[cnt];
  struct ibv_mr *mr;

  int ret;
  unsigned sent =
      0; // Used for first loop: prepare work requests to post to send queue

  n->logger->debug("ib_write is called");

  if (n->getState() == State::CONNECTED) {
    // TODO: fix release logic
    // *release = 0;

    // First, write

    // Get Memory Region
    mr = memory::ib_get_mr(pool_buffer(sample_pool(smps[0])));

    for (sent = 0; sent < cnt; sent++) {
      int j = 0;

      // Set Scatter/Gather element to data of sample

      // Sequence
      sge[sent][j].addr = (uint64_t)&smps[sent]->sequence;
      sge[sent][j].length = sizeof(smps[sent]->sequence);
      sge[sent][j].lkey = mr->lkey;

      j++;

      // Timespec origin
      sge[sent][j].addr = (uint64_t)&smps[sent]->ts.origin;
      sge[sent][j].length = sizeof(smps[sent]->ts.origin);
      sge[sent][j].lkey = mr->lkey;

      j++;

      // Actual Payload
      sge[sent][j].addr = (uint64_t)&smps[sent]->data;
      sge[sent][j].length = SAMPLE_DATA_LENGTH(smps[sent]->length);
      sge[sent][j].lkey = mr->lkey;

      j++;

      // Check if connection is connected or unconnected and set appropriate values
      if (ib->conn.port_space == RDMA_PS_UDP) {
        wr[sent].wr.ud.ah = ib->conn.ud.ah;
        wr[sent].wr.ud.remote_qkey = ib->conn.ud.ud.qkey;
        wr[sent].wr.ud.remote_qpn = ib->conn.ud.ud.qp_num;
      }

      /* Check if data can be send inline
			 * 32 byte meta data is always send.
			 * Once every max_send_wr iterations a signal must be generated. Since we would need
			 * an additional buffer if we were sending inlines with IBV_SEND_SIGNALED, we prefer
			 * to send one samples every max_send_wr NOT inline (which thus generates a signal).
			 */
      int send_inline =
          ((sge[sent][j - 1].length + META_SIZE) <
           ib->qp_init.cap.max_inline_data) &&
                  ((++ib->signaling_counter % ib->periodic_signaling) != 0)
              ? ib->conn.send_inline
              : 0;

      n->logger->debug("Sample will be send inline [0/1]: {}", send_inline);

      // Set Send Work Request
      wr[sent].wr_id = (uintptr_t)smps[sent];
      wr[sent].sg_list = sge[sent];
      wr[sent].num_sge = j;
      wr[sent].next = &wr[sent + 1];

      wr[sent].send_flags = send_inline ? IBV_SEND_INLINE : IBV_SEND_SIGNALED;
      wr[sent].opcode = IBV_WR_SEND;
    }

    n->logger->debug("Prepared {} send Work Requests", cnt);
    wr[cnt - 1].next = nullptr;

    // Send linked list of Work Requests
    ret = ibv_post_send(ib->ctx.id->qp, wr, &bad_wr);
    n->logger->debug("Posted send Work Requests");

    /* Reorder list. Place inline and unposted samples to the top
		 * m will always be equal or smaller than *release
		 */
    for (unsigned m = 0; m < cnt; m++) {
      /* We can't use wr_id as identifier, since it is 0 for inline
			 * elements
			 */
      if (ret && (wr[m].sg_list == bad_wr->sg_list)) {
        /* The remaining work requests will be bad. Ripple through list
				 * and prepare them to be released
				 */
        n->logger->debug(
            "Bad WR occured with ID: {:#x} and S/G address: {:#x}: {}",
            bad_wr->wr_id, (void *)bad_wr->sg_list, ret);

        while (1) {
          // TODO: fix release logic
          // smps[*release] = smps[m];
          // (*release)++; // Increment number of samples to be released
          sent--; // Decrement the number of successfully posted elements

          if (++m == cnt)
            break;
        }
      } else if (wr[m].send_flags & IBV_SEND_INLINE) {
        // TODO: fix release logic
        // smps[*release] = smps[m];
        // (*release)++;
      }
    }

    // TODO: fix release logic
    // n->logger->debug("{} samples will be released (before WC)", *release);

    // Try to grab as many CQEs from CQ as there is space in *smps[]
    // ret = ibv_poll_cq(ib->ctx.send_cq, cnt - *release, wc);

    for (int i = 0; i < ret; i++) {
      if (wc[i].status != IBV_WC_SUCCESS && wc[i].status != IBV_WC_WR_FLUSH_ERR)
        n->logger->warn("Work Completion status was not IBV_WC_SUCCESS: {}",
                        (int)wc[i].status);

      // TODO: fix release logic
      // smps[*release] = (struct Sample *) (wc[i].wr_id);
      // (*release)++;
    }

    // TODO: fix release logic
    // n->logger->debug("{} samples will be released (after WC)", *release);
  }

  return sent;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "infiniband";
  p.description = "Infiniband interface (libibverbs, librdmacm)";
  p.vectorize = 0;
  p.size = sizeof(struct infiniband);
  // TODO: fix
  // p.pool_size	= 8192;
  p.destroy = ib_destroy;
  p.parse = ib_parse;
  p.check = ib_check;
  p.print = ib_print;
  p.start = ib_start;
  p.stop = ib_stop;
  p.read = ib_read;
  p.write = ib_write;
  p.reverse = ib_reverse;
  p.memory_type = memory::ib;

  static NodeCompatFactory ncp(&p);
}
