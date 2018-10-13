/** Node type: infiniband
 *
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

#include <string.h>
#include <math.h>

#include <villas/nodes/infiniband.h>
#include <villas/plugin.h>
#include <villas/utils.h>
#include <villas/format_type.h>
#include <villas/memory.h>
#include <villas/memory/ib.h>
#include <villas/timing.h>

static int ib_disconnect(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_wc wc[MAX(ib->recv_cq_size, ib->send_cq_size)];
	int wcs;
	debug(LOG_IB | 1, "Starting to clean up");

	rdma_disconnect(ib->ctx.id);

	// If there is anything in the Completion Queue, it should be given back to the framework
	// Receive Queue
	while (ib->conn.available_recv_wrs) {
		wcs = ibv_poll_cq(ib->ctx.recv_cq, ib->recv_cq_size, wc);

		ib->conn.available_recv_wrs -= wcs;

		for (int j = 0; j < wcs; j++)
			sample_decref((struct sample *) (wc[j].wr_id));
	}

	// Send Queue
	while ((wcs = ibv_poll_cq(ib->ctx.send_cq, ib->send_cq_size, wc)))
		for (int j = 0; j < wcs; j++)
			if (wc[j].wr_id > 0)
				sample_decref((struct sample *) (wc[j].wr_id));

	// Send Queue Stack
	struct sample *smp = NULL;
	while (queue_available(&ib->conn.send_wc_buffer)) {
		// Because of queue_available, queue_pull should always return. No need
		// to double check return of queue_pull.
		queue_pull(&ib->conn.send_wc_buffer, (void **) &smp);

		sample_decref(smp);
	}

	// Destroy QP
	rdma_destroy_qp(ib->ctx.id);
	debug(LOG_IB | 3, "Destroyed QP");

	return ib->stopThreads;
}

static void ib_build_ibv(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	debug(LOG_IB | 1, "Starting to build IBV components");

	// Create completion queues. No completion channel!)
	ib->ctx.recv_cq = ibv_create_cq(ib->ctx.id->verbs, ib->recv_cq_size, NULL, NULL, 0);
	if (!ib->ctx.recv_cq)
		error("Could not create receive completion queue in node %s", node_name(n));

	debug(LOG_IB | 3, "Created receive Completion Queue");

	ib->ctx.send_cq = ibv_create_cq(ib->ctx.id->verbs, ib->send_cq_size, NULL, NULL, 0);
	if (!ib->ctx.send_cq)
		error("Could not create send completion queue in node %s", node_name(n));

	debug(LOG_IB | 3, "Created send Completion Queue");

	// Prepare remaining Queue Pair (QP) attributes
	ib->qp_init.send_cq = ib->ctx.send_cq;
	ib->qp_init.recv_cq = ib->ctx.recv_cq;

	// Create the actual QP
	ret = rdma_create_qp(ib->ctx.id, ib->ctx.pd, &ib->qp_init);
	if (ret)
		error("Failed to create Queue Pair in node %s", node_name(n));

	debug(LOG_IB | 3, "Created Queue Pair with %i receive and %i send elements",
		ib->qp_init.cap.max_recv_wr, ib->qp_init.cap.max_send_wr);

	if (ib->conn.send_inline)
		info("Maximum inline size is set to %i byte", ib->qp_init.cap.max_inline_data);
}

static int ib_addr_resolved(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	debug(LOG_IB | 1, "Successfully resolved address");

	// Build all components from IB Verbs
	ib_build_ibv(n);

	// Resolve address
	ret = rdma_resolve_route(ib->ctx.id, ib->conn.timeout);
	if (ret)
		error("Failed to resolve route in node %s", node_name(n));

	return 0;
}

static int ib_route_resolved(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	struct rdma_conn_param cm_params;
	memset(&cm_params, 0, sizeof(cm_params));

	// Send connection request
	ret = rdma_connect(ib->ctx.id, &cm_params);
	if (ret)
		error("Failed to connect in node %s", node_name(n));

	debug(LOG_IB | 1, "Called rdma_connect");

	return 0;
}

static int ib_connect_request(struct node *n, struct rdma_cm_id *id)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;
	debug(LOG_IB | 1, "Received a connection request!");

	ib->ctx.id = id;
	ib_build_ibv(n);

	struct rdma_conn_param cm_params;
	memset(&cm_params, 0, sizeof(cm_params));

	// Accept connection request
	ret = rdma_accept(ib->ctx.id, &cm_params);
	if (ret)
		error("Failed to connect in node %s", node_name(n));

	info("Successfully accepted connection request in node %s", node_name(n));

	return 0;
}

int ib_reverse(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;

	SWAP(ib->conn.src_addr, ib->conn.dst_addr);

	return 0;
}

int ib_parse(struct node *n, json_t *cfg)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;

	int ret;
	char *local = NULL;
	char *remote = NULL;
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
	json_t *json_in = NULL;
	json_t *json_out = NULL;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{s?: o, s?: o, s?: s, s?: s}",
		"in", &json_in,
		"out", &json_out,
		"rdma_transport_mode", &transport_mode
	);
	if (ret)
		jerror(&err, "Failed to parse in/out json blocks");


	if (json_in) {
		ret = json_unpack_ex(json_in, &err, 0, "{ s?: s, s?: i, s?: i, s?: i, s?: i}",
			"address", &local,
			"cq_size", &recv_cq_size,
			"max_wrs", &max_recv_wr,
			"vectorize", &vectorize_in,
			"buffer_subtraction", &buffer_subtraction
		);
		if (ret)
			jerror(&err, "Failed to parse input configuration of node %s", node_name(n));
	}

	if (json_out) {
		ret = json_unpack_ex(json_out, &err, 0, "{ s?: s, s?: i, s?: i, s?: i, s?: i, s?: b, s?: i, s?: b}",
			"address", &remote,
			"resolution_timeout", &timeout,
			"cq_size", &send_cq_size,
			"max_wrs", &max_send_wr,
			"max_inline_data", &max_inline_data,
			"send_inline", &send_inline,
			"vectorize", &vectorize_out,
			"use_fallback", &use_fallback
		);
		if (ret)
			jerror(&err, "Failed to parse output configuration of node %s", node_name(n));

		if (remote) {
			ib->is_source = 1;

			debug(LOG_IB | 3, "Node %s is up as source and target", node_name(n));
		}
	}
	else {
		ib->is_source = 0;

		debug(LOG_IB | 3, "Node %s is up as target", node_name(n));
	}

	// Set fallback mode
	ib->conn.use_fallback = use_fallback;

	// Set vectorize mode. Do not print, since framework will print this information
	n->in.vectorize = vectorize_in;
	n->out.vectorize = vectorize_out;

	// Set buffer subtraction
	ib->conn.buffer_subtraction = buffer_subtraction;

	debug(LOG_IB | 4, "Set buffer subtraction to %i in node %s", buffer_subtraction, node_name(n));

	// Translate IP:PORT to a struct addrinfo
	char* ip_adr = strtok(local, ":");
	char* port = strtok(NULL, ":");

	ret = getaddrinfo(ip_adr, port, NULL, &ib->conn.src_addr);
	if (ret)
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), gai_strerror(ret));

	debug(LOG_IB | 4, "Translated %s:%s to a struct addrinfo in node %s", ip_adr, port, node_name(n));

	// Translate port space
	if (strcmp(transport_mode, "RC") == 0) {
		ib->conn.port_space = RDMA_PS_TCP;
		ib->qp_init.qp_type = IBV_QPT_RC;
	}
	else if (strcmp(transport_mode, "UC") == 0) {
#ifdef RDMA_CMA_H_CUSTOM
		ib->conn.port_space = RDMA_PS_IB;
		ib->qp_init.qp_type = IBV_QPT_UC;
#else
		error("Unreliable Connected (UC) mode is only available with an adapted version of librdma. "
			"Please read the Infiniband node type Documentation for more information on UC!");
#endif
	}
	else if (strcmp(transport_mode, "UD") == 0) {
		ib->conn.port_space = RDMA_PS_UDP;
		ib->qp_init.qp_type = IBV_QPT_UD;
	}
	else
		error("transport_mode = %s is not a valid transport mode in node %s!",
				transport_mode, node_name(n));

	debug(LOG_IB | 4, "Set transport mode to %s in node %s", transport_mode, node_name(n));

	// Set timeout
	ib->conn.timeout = timeout;

	debug(LOG_IB | 4, "Set  timeout to %i in node %s", timeout, node_name(n));

	// Set completion queue size
	ib->recv_cq_size = recv_cq_size;
	ib->send_cq_size = send_cq_size;

	debug(LOG_IB | 4, "Set Completion Queue size to %i & %i (in & out) in node %s",
			recv_cq_size, send_cq_size, node_name(n));


	// Translate inline mode
	ib->conn.send_inline = send_inline;

	debug(LOG_IB | 4, "Set send_inline to %i in node %s", send_inline, node_name(n));

	// Set max. send and receive Work Requests
	ib->qp_init.cap.max_send_wr = max_send_wr;
	ib->qp_init.cap.max_recv_wr = max_recv_wr;

	debug(LOG_IB | 4, "Set max_send_wr and max_recv_wr in node %s to %i and %i, respectively",
			node_name(n), max_send_wr, max_recv_wr);

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
		char* ip_adr = strtok(remote, ":");
		char* port = strtok(NULL, ":");

		ret = getaddrinfo(ip_adr, port, NULL, &ib->conn.dst_addr);
		if (ret)
			error("Failed to resolve remote address '%s' of node %s: %s",
				remote, node_name(n), gai_strerror(ret));

		debug(LOG_IB | 4, "Translated %s:%s to a struct addrinfo", ip_adr, port);
	}

	return 0;
}

int ib_check(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;

	info("Starting check of node %s", node_name(n));

	// Check if read substraction makes sense
	if (ib->conn.buffer_subtraction <  2 * n->in.vectorize)
		error("The buffer substraction value must be bigger than 2 * in.vectorize");

	if (ib->conn.buffer_subtraction >= ib->qp_init.cap.max_recv_wr - n->in.vectorize)
		error("The buffer substraction value cannot be bigger than in.max_wrs - in.vectorize");

	// Check if the set value is a power of 2, and warn the user if this is not the case
	int max_send_pow = (int) pow(2, ceil(log2(ib->qp_init.cap.max_send_wr)));
	int max_recv_pow = (int) pow(2, ceil(log2(ib->qp_init.cap.max_recv_wr)));

	if (ib->qp_init.cap.max_send_wr != max_send_pow) {
		warn("Max nr. of send WRs (%i) is not a power of 2! It will be changed to a power of 2: %i",
			ib->qp_init.cap.max_send_wr, max_send_pow);

		// Change it now, because otherwise errors are possible in ib_start().
		ib->qp_init.cap.max_send_wr = max_send_pow;
	}

	if (ib->qp_init.cap.max_recv_wr != max_recv_pow) {
		warn("Max nr. of recv WRs (%i) is not a power of 2! It will be changed to a power of 2: %i",
			ib->qp_init.cap.max_recv_wr, max_recv_pow);

		// Change it now, because otherwise errors are possible in ib_start().
		ib->qp_init.cap.max_recv_wr = max_recv_pow;
	}

	// Check maximum size of max_recv_wr and max_send_wr
	if (ib->qp_init.cap.max_send_wr > 8192)
		warn("Max number of send WRs (%i) is bigger than send queue!", ib->qp_init.cap.max_send_wr);

	if (ib->qp_init.cap.max_recv_wr > 8192)
		warn("Max number of receive WRs (%i) is bigger than send queue!", ib->qp_init.cap.max_recv_wr);

	// Warn user if he changed the default inline value
	if (ib->qp_init.cap.max_inline_data != 0)
		warn("You changed the default value of max_inline_data. This might influence the maximum number "
			"of outstanding Work Requests in the Queue Pair and can be a reason for the Queue Pair creation to fail");

	info("Finished check of node %s", node_name(n));

	return 0;
}

char * ib_print(struct node *n)
{
	return 0;
}

int ib_destroy(struct node *n)
{
	return 0;
}

static void ib_create_bind_id(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	// Create rdma_cm_id
	/**
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
	ret = rdma_create_id2(ib->ctx.ec, &ib->ctx.id, NULL, ib->conn.port_space, ib->qp_init.qp_type);
#else
	ret = rdma_create_id(ib->ctx.ec, &ib->ctx.id, NULL, ib->conn.port_space);
#endif
	if (ret)
		error("Failed to create rdma_cm_id of node %s: %s", node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Created rdma_cm_id");

	// Bind rdma_cm_id to the HCA
	ret = rdma_bind_addr(ib->ctx.id, ib->conn.src_addr->ai_addr);
	if (ret)
		error("Failed to bind to local device of node %s: %s",
			node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Bound rdma_cm_id to Infiniband device");

	// The ID will be overwritten for the target. If the event type is
	// RDMA_CM_EVENT_CONNECT_REQUEST, >then this references a new id for
	// that communication.
	ib->ctx.listen_id = ib->ctx.id;
}

static void ib_continue_as_listen(struct node *n, struct rdma_cm_event *event)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	if (ib->conn.use_fallback)
		warn("Trying to continue as listening node");
	else
		error("Cannot establish a connection with remote host! If you want that %s tries to "
			"continue as listening node in such cases, set use_fallback = true in the configuration",
			node_name(n));

	n->state = STATE_STARTED;

	// Acknowledge event
	rdma_ack_cm_event(event);

	// Destroy ID
	rdma_destroy_id(ib->ctx.listen_id);

	// Create rdma_cm_id and bind to device
	ib_create_bind_id(n);

	// Listen to id for events
	ret = rdma_listen(ib->ctx.listen_id, 10);
	if (ret)
		error("Failed to listen to rdma_cm_id on node %s", node_name(n));

	// Node is not a source (and will not send data
	ib->is_source = 0;

	info("Node %s is set to listening mode", node_name(n));
}

void * ib_rdma_cm_event_thread(void *n)
{
	struct node *node = (struct node *) n;
	struct infiniband *ib = (struct infiniband *) node->_vd;
	struct rdma_cm_event *event;
	int ret = 0;

	debug(LOG_IB | 1, "Started rdma_cm_event thread of node %s", node_name(node));

	// Wait until node is completely started
	while (node->state != STATE_STARTED);

	// Monitor event channel
	while (rdma_get_cm_event(ib->ctx.ec, &event) == 0) {
		debug(LOG_IB | 2, "Received communication event: %s", rdma_event_str(event->event));

		switch(event->event) {
			case RDMA_CM_EVENT_ADDR_RESOLVED:
				ret = ib_addr_resolved(n);
				break;

			case RDMA_CM_EVENT_ADDR_ERROR:
				warn("Address resolution (rdma_resolve_addr) failed!");

				ib_continue_as_listen(n, event);

				break;

			case RDMA_CM_EVENT_ROUTE_RESOLVED:
				ret = ib_route_resolved(n);
				break;

			case RDMA_CM_EVENT_ROUTE_ERROR:
				warn("Route resolution (rdma_resovle_route) failed!");

				ib_continue_as_listen(n, event);

				break;

			case RDMA_CM_EVENT_UNREACHABLE:
				warn("Remote server unreachable!");

				ib_continue_as_listen(n, event);
				break;

			case RDMA_CM_EVENT_CONNECT_REQUEST:
				ret = ib_connect_request(n, event->id);

				// A target UDP node will never really connect. In order to receive data,
				// we set it to connected after it answered the connection request
				// with rdma_connect.
				if (ib->conn.port_space == RDMA_PS_UDP && !ib->is_source)
					node->state = STATE_CONNECTED;
				else
					node->state = STATE_PENDING_CONNECT;

				break;

			case RDMA_CM_EVENT_CONNECT_ERROR:
				warn("An error has occurred trying to establish a connection!");

				ib_continue_as_listen(n, event);

				break;

			case RDMA_CM_EVENT_REJECTED:
				warn("Connection request or response was rejected by the remote end point!");

				ib_continue_as_listen(n, event);

				break;

			case RDMA_CM_EVENT_ESTABLISHED:
				// If the connection is unreliable connectionless, set appropriate variables
				if (ib->conn.port_space == RDMA_PS_UDP) {
					ib->conn.ud.ud = event->param.ud;
					ib->conn.ud.ah = ibv_create_ah(ib->ctx.pd, &ib->conn.ud.ud.ah_attr);
				}

				node->state = STATE_CONNECTED;

				info("Connection established in node %s", node_name(n));

				break;

			case RDMA_CM_EVENT_DISCONNECTED:
				node->state = STATE_STARTED;

				ret = ib_disconnect(n);

				if (!ret)
					info("Host disconnected. Ready to accept new connections.");

				break;

			case RDMA_CM_EVENT_TIMEWAIT_EXIT:
				break;

			default:
				error("Unknown event occurred: %u", event->event);
		}

		rdma_ack_cm_event(event);

		if (ret)
			break;
	}

	return NULL;
}

int ib_start(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	debug(LOG_IB | 1, "Started ib_start");

	// Create event channel
	ib->ctx.ec = rdma_create_event_channel();
	if (!ib->ctx.ec)
		error("Failed to create event channel in node %s!", node_name(n));

	debug(LOG_IB | 3, "Created event channel");

	// Create rdma_cm_id and bind to device
	ib_create_bind_id(n);

	// Initialize send Work Completion queue
	queue_init(&ib->conn.send_wc_buffer, ib->qp_init.cap.max_send_wr, &memory_heap);

	debug(LOG_IB | 3, "Initialized Work Completion Buffer");

	// Resolve address or listen to rdma_cm_id
	if (ib->is_source) {
		// Resolve address
		ret = rdma_resolve_addr(ib->ctx.id, NULL, ib->conn.dst_addr->ai_addr, ib->conn.timeout);
		if (ret)
			error("Failed to resolve remote address after %ims of node %s: %s",
				ib->conn.timeout, node_name(n), gai_strerror(ret));
	}
	else {
		// Listen on rdma_cm_id for events
		ret = rdma_listen(ib->ctx.listen_id, 10);
		if (ret)
			error("Failed to listen to rdma_cm_id on node %s", node_name(n));

		debug(LOG_IB | 3, "Started to listen to rdma_cm_id");
	}

	//Allocate protection domain
	ib->ctx.pd = ibv_alloc_pd(ib->ctx.id->verbs);
	if (!ib->ctx.pd)
		error("Could not allocate protection domain in node %s", node_name(n));

	debug(LOG_IB | 3, "Allocated Protection Domain");

	// Allocate space for 40 Byte GHR. We don't use this.
	if (ib->conn.port_space == RDMA_PS_UDP) {
		ib->conn.ud.grh_ptr = alloc(GRH_SIZE);
		ib->conn.ud.grh_mr = ibv_reg_mr(ib->ctx.pd, ib->conn.ud.grh_ptr, GRH_SIZE, IBV_ACCESS_LOCAL_WRITE);
	}

	// Several events should occur on the event channel, to make
	// sure the nodes are succesfully connected.
	debug(LOG_IB | 1, "Starting to monitor events on rdma_cm_id");

	//Create thread to monitor rdma_cm_event channel
	ret = pthread_create(&ib->conn.rdma_cm_event_thread, NULL, ib_rdma_cm_event_thread, n);
	if (ret)
		error("Failed to create thread to monitor rdma_cm events in node %s: %s",
			node_name(n), gai_strerror(ret));

	return 0;
}

int ib_stop(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	debug(LOG_IB | 1, "Called ib_stop");

	ib->stopThreads = 1;

	// Call RDMA disconnect function
	// Will flush all outstanding WRs to the Completion Queue and
	// will call RDMA_CM_EVENT_DISCONNECTED if that is done.
	if (n->state == STATE_CONNECTED && ib->conn.port_space != RDMA_PS_UDP) {
		ret = rdma_disconnect(ib->ctx.id);

		if (ret)
			error("Error while calling rdma_disconnect in node %s: %s",
				node_name(n), gai_strerror(ret));

		debug(LOG_IB | 3, "Called rdma_disconnect");
	}
	else {
		pthread_cancel(ib->conn.rdma_cm_event_thread);

		debug(LOG_IB | 3, "Called pthread_cancel() on communication management thread.");
	}

	info("Disconnecting... Waiting for threads to join.");

	// Wait for event thread to join
	ret = pthread_join(ib->conn.rdma_cm_event_thread, NULL);
	if (ret)
		error("Error while joining rdma_cm_event_thread in node %s: %i", node_name(n), ret);

	debug(LOG_IB | 3, "Joined rdma_cm_event_thread");

	// Destroy RDMA CM ID
	rdma_destroy_id(ib->ctx.id);
	debug(LOG_IB | 3, "Destroyed rdma_cm_id");

	// Dealloc Protection Domain
	ibv_dealloc_pd(ib->ctx.pd);
	debug(LOG_IB | 3, "Destroyed protection domain");

	// Destroy event channel
	rdma_destroy_event_channel(ib->ctx.ec);
	debug(LOG_IB | 3, "Destroyed event channel");

	info("Successfully stopped %s", node_name(n));

	return 0;
}

int ib_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_wc wc[cnt];
	struct ibv_recv_wr wr[cnt], *bad_wr = NULL;
    	struct ibv_sge sge[cnt][ib->qp_init.cap.max_recv_sge];
	struct ibv_mr *mr;
	struct timespec ts_receive;
	int ret = 0, wcs = 0, read_values = 0, max_wr_post;

	debug(LOG_IB | 15, "ib_read is called");

	if (n->state == STATE_CONNECTED || n->state == STATE_PENDING_CONNECT) {

		max_wr_post = cnt;

		// Poll Completion Queue
		// If we've already posted enough receive WRs, try to pull cnt
		if (ib->conn.available_recv_wrs >= (ib->qp_init.cap.max_recv_wr - ib->conn.buffer_subtraction) ) {
			for (int i = 0;; i++) {
				if (i % CHK_PER_ITER == CHK_PER_ITER - 1) pthread_testcancel();

				// If IB node disconnects or if it is still in STATE_PENDING_CONNECT, ib_read
				// should return immediately if this condition holds
				if (n->state != STATE_CONNECTED) return 0;

				wcs = ibv_poll_cq(ib->ctx.recv_cq, cnt, wc);
				if (wcs) {
					// Get time directly after something arrived in Completion Queue
					ts_receive = time_now();

					debug(LOG_IB | 10, "Received %i Work Completions", wcs);

					read_values = wcs; // Value to return
					max_wr_post = wcs; // Make space free in smps[]

					break;
				}
			}

			// All samples (wcs * received + unposted) should be released. Let
			// *release be equal to allocated.
			//
			// This is set in the framework, before this function was called
		}
		else {
			ib->conn.available_recv_wrs += max_wr_post;
			*release = 0; // While we fill the receive queue, we always use all samples
		}

		// Get Memory Region
		mr = memory_ib_get_mr(pool_buffer(sample_pool(smps[0])));

		for (int i = 0; i < max_wr_post; i++) {
			int j = 0;

			// Prepare receive Scatter/Gather element

			// First 40 byte of UD data are GRH and unused in our case
			if (ib->conn.port_space == RDMA_PS_UDP) {
				sge[i][j].addr = (uint64_t) ib->conn.ud.grh_ptr;
    				sge[i][j].length = GRH_SIZE;
    				sge[i][j].lkey = ib->conn.ud.grh_mr->lkey;

				j++;
			}

			// Sequence
			sge[i][j].addr = (uint64_t) &smps[i]->sequence;
			sge[i][j].length = sizeof(smps[i]->sequence);
			sge[i][j].lkey = mr->lkey;

			j++;

			// Timespec origin
			sge[i][j].addr = (uint64_t) &smps[i]->ts.origin;
			sge[i][j].length = sizeof(smps[i]->ts.origin);
			sge[i][j].lkey = mr->lkey;

			j++;

    			sge[i][j].addr = (uint64_t) &smps[i]->data;
    			sge[i][j].length = SAMPLE_DATA_LENGTH(DEFAULT_SAMPLE_LENGTH);
    			sge[i][j].lkey = mr->lkey;

			j++;

    			// Prepare a receive Work Request
    			wr[i].wr_id = (uintptr_t) smps[i];
			wr[i].next = &wr[i+1];
    			wr[i].sg_list = sge[i];
    			wr[i].num_sge = j;
		}

		wr[max_wr_post-1].next = NULL;

		debug(LOG_IB | 5, "Prepared %i new receive Work Requests", max_wr_post);
		debug(LOG_IB | 5, "%i receive Work Requests in Receive Queue", ib->conn.available_recv_wrs);

		// Post list of Work Requests
		ret = ibv_post_recv(ib->ctx.id->qp, &wr[0], &bad_wr);

		if (ret)
			error("Was unable to post receive WR in node %s: %i, bad WR ID: 0x%lx",
			node_name(n), ret, bad_wr->wr_id);

		debug(LOG_IB | 10, "Succesfully posted receive Work Requests");

		// Doesn't start if wcs == 0
		for (int j = 0; j < wcs; j++) {
			if ( !( (wc[j].opcode & IBV_WC_RECV) && wc[j].status == IBV_WC_SUCCESS) ) {
				// Drop all values, we don't know where the error occured
				read_values = 0;
			}

			if (wc[j].status == IBV_WC_WR_FLUSH_ERR)
				debug(LOG_IB | 5, "Received IBV_WC_WR_FLUSH_ERR (ib_read). Ignore it.");
			else if (wc[j].status != IBV_WC_SUCCESS)
				warn("Work Completion status was not IBV_WC_SUCCES in node %s: %i",
					node_name(n), wc[j].status);

			// 32 byte of meta data is always transferred. We should substract it.
			// Furthermore, in case of an unreliable connection, a 40 byte
			// global routing header is transferred. This should be substracted as well.
			int correction = (ib->conn.port_space == RDMA_PS_UDP) ? META_GRH_SIZE : META_SIZE;

			smps[j] = (struct sample *) (wc[j].wr_id);

			smps[j]->length = SAMPLE_NUMBER_OF_VALUES(wc[j].byte_len - correction);
			smps[j]->ts.received = ts_receive;

			printf("time_ns: %lu\n", smps[j]->sequence);
			smps[j]->flags = (SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_TS_RECEIVED | SAMPLE_HAS_SEQUENCE);
		}

	}
	return read_values;
}

int ib_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_send_wr wr[cnt], *bad_wr = NULL;
	struct ibv_sge sge[cnt][ib->qp_init.cap.max_recv_sge];
	struct ibv_wc wc[cnt];
	struct ibv_mr *mr;

	int ret;
	int sent = 0; //Used for first loop: prepare work requests to post to send queue

	debug(LOG_IB | 10, "ib_write is called");

	if (n->state == STATE_CONNECTED) {
		*release = 0;

		// First, write

		// Get Memory Region
		mr = memory_ib_get_mr(pool_buffer(sample_pool(smps[0])));

		for (sent = 0; sent < cnt; sent++) {
			int j = 0;

			// Set Scatter/Gather element to data of sample
			// Sequence
			sge[sent][j].addr = (uint64_t) &smps[sent]->sequence;
			sge[sent][j].length = sizeof(smps[sent]->sequence);
			sge[sent][j].lkey = mr->lkey;

			j++;

			// Timespec origin
			sge[sent][j].addr = (uint64_t) &smps[sent]->ts.origin;
			sge[sent][j].length = sizeof(smps[sent]->ts.origin);
			sge[sent][j].lkey = mr->lkey;

			j++;

			// Actual Payload
			sge[sent][j].addr = (uint64_t) &smps[sent]->data;
			sge[sent][j].length = SAMPLE_DATA_LENGTH(smps[sent]->length);
			sge[sent][j].lkey = mr->lkey;

			j++;

			// Check if connection is connected or unconnected and set appropriate values
			if (ib->conn.port_space == RDMA_PS_UDP) {
				wr[sent].wr.ud.ah = ib->conn.ud.ah;
				wr[sent].wr.ud.remote_qkey = ib->conn.ud.ud.qkey;
				wr[sent].wr.ud.remote_qpn = ib->conn.ud.ud.qp_num;
			}

			// Check if data can be send inline
			// 32 byte meta data is always send.
			int send_inline = ( (sge[sent][j-1].length + META_SIZE) < ib->qp_init.cap.max_inline_data) ?
						ib->conn.send_inline : 0;


			debug(LOG_IB | 10, "Sample will be send inline [0/1]: %i", send_inline);

			// Set Send Work Request
			wr[sent].wr_id = send_inline ? 0 : (uintptr_t) smps[sent]; // This way the sample can be release in WC
			wr[sent].sg_list = sge[sent];
			wr[sent].num_sge = j;
			wr[sent].next = &wr[sent+1];

			wr[sent].send_flags = IBV_SEND_SIGNALED | (send_inline << 3);
			wr[sent].opcode = IBV_WR_SEND;
		}

		debug(LOG_IB | 10, "Prepared %i send Work Requests", cnt);
		wr[cnt-1].next = NULL;

		// Send linked list of Work Requests
		ret = ibv_post_send(ib->ctx.id->qp, wr, &bad_wr);
		debug(LOG_IB | 4, "Posted send Work Requests");

		// Reorder list. Place inline and unposted samples to the top
		// m will always be equal or smaller than *release
		for (int m = 0; m < cnt; m++) {
			// We can't use wr_id as identifier, since it is 0 for inline
			// elements
			if (ret && (wr[m].sg_list == bad_wr->sg_list)) {
				// The remaining work requests will be bad. Ripple through list
				// and prepare them to be released
				debug(LOG_IB | 4, "Bad WR occured with ID: 0x%lx and S/G address: 0x%px: %i",
						bad_wr->wr_id, bad_wr->sg_list, ret);

				while (1) {
					smps[*release] = smps[m];

					(*release)++; // Increment number of samples to be released
					sent--; // Decrement the number of succesfully posted elements

					if (++m == cnt) break;
				}
			}
			else if (wr[m].send_flags & IBV_SEND_INLINE) {
				smps[*release] = smps[m];

				(*release)++;
			}

		}

		debug(LOG_IB | 4, "%i samples will be released (before WC)", *release);

		// Always poll cnt items from Receive Queue. If there is not enough space in
		// smps, we temporarily save it in a queue
		ret = ibv_poll_cq(ib->ctx.send_cq, cnt, wc);

		for (int i = 0; i < ret; i++) {
			if (wc[i].status != IBV_WC_SUCCESS && wc[i].status != IBV_WC_WR_FLUSH_ERR)
				warn("Work Completion status was not IBV_WC_SUCCES in node %s: %i",
					node_name(n), wc[i].status);

			// Release only samples which were not send inline
			if (wc[i].wr_id) {
				if (cnt - *release > 0) {
					smps[*release] = (struct sample *) (wc[i].wr_id);
					(*release)++;
				}
				else {
					queue_push(&ib->conn.send_wc_buffer, (struct sample *) (wc[i].wr_id));
					debug(LOG_IB | 10, "Push in send WC Queue: %px", (struct sample *) (wc[i].wr_id));
				}
			}
		}
		// Check if we still have some space and try to get rid of some addresses in our queue
		if (queue_available(&ib->conn.send_wc_buffer)) {
			int empty_smps = cnt - *release;
			for (int i = 0; i < empty_smps; i++) {
				ret = queue_pull(&ib->conn.send_wc_buffer, (void **) &smps[*release]);
				debug(LOG_IB | 10, "Pull from send WC Queue: %px", (struct sample *) smps[*release]);

				if (ret)
					(*release)++;
				else
					break;
			}
		}

		debug(LOG_IB | 4, "%i samples will be released (after WC)", *release);
	}

	return sent;
}

int ib_fd(struct node *n)
{
	return 0;
}

static struct plugin p = {
	.name		= "infiniband",
	.description	= "Infiniband interface (libibverbs, librdmacm)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct infiniband),
		.pool_size	= 8192,
		.reverse	= ib_reverse,
		.parse		= ib_parse,
		.check		= ib_check,
		.print		= ib_print,
		.start		= ib_start,
		.destroy	= ib_destroy,
		.stop		= ib_stop,
		.read		= ib_read,
		.write		= ib_write,
		.fd		= ib_fd,
		.memory_type	= memory_ib
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
