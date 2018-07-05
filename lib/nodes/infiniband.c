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
#include <villas/pool.h>
#include <villas/memory.h>
#include <villas/memory/ib.h>

static struct rdma_event_channel *rdma_event_channel;

int ib_cleanup(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	debug(LOG_IB | 1, "Starting to clean up");

	// Destroy QP
	rdma_destroy_qp(ib->ctx.id);
	debug(LOG_IB | 3, "Destroyed QP");

	// Deregister memory regions
	ibv_dereg_mr(ib->mem.mr_recv);
	if (ib->is_source)
		ibv_dereg_mr(ib->mem.mr_send);
	debug(LOG_IB | 3, "Deregistered memory regions");

	// Destroy pools
	pool_destroy(&ib->mem.p_recv);
	pool_destroy(&ib->mem.p_send);
	debug(LOG_IB | 3, "Destroyed memory pools");

	// Destroy RDMA CM ID
	rdma_destroy_id(ib->ctx.id);
	debug(LOG_IB | 3, "Destroyed rdma_cm_id");

	// Destroy event channel
	rdma_destroy_event_channel(rdma_event_channel);
	debug(LOG_IB | 3, "Destroyed event channel");

	return 0;
}

int ib_post_recv_wrs(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_recv_wr wr, *bad_wr = NULL;
	int ret;
	struct ibv_sge sge;

	// Prepare receive Scatter/Gather element
	sge.addr = (uintptr_t) pool_get(&ib->mem.p_recv);
	sge.length = ib->mem.p_recv.blocksz;
	sge.lkey = ib->mem.mr_recv->lkey;

	// Prepare a receive Work Request
	wr.wr_id = (uintptr_t) sge.addr;
	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;

	// Post Work Request
	ret = ibv_post_recv(ib->ctx.id->qp, &wr, &bad_wr);

	return ret;
}

void ib_completion_target(struct node* n, struct ibv_wc* wc, int* size){}

void ib_completion_source(struct node* n, struct ibv_wc* wc, int* size)
{
	struct infiniband *ib = (struct infiniband *) ((struct node *) n)->_vd;

	for (int i = 0; i < *size; i++)	{
		//On disconnect, the QP set to error state and will be flushed
		if (wc[i].status == IBV_WC_WR_FLUSH_ERR) {
			debug(LOG_IB | 5, "Received IBV_WC_WR_FLUSH_ERR in ib_completion_source. Stopping thread.");

			ib->poll.stopThread = 1;
			return;
		}

		if (wc[i].status != IBV_WC_SUCCESS)
			warn("Work Completion status was not IBV_WC_SUCCES in node %s: %i",
				node_name(n), wc[i].status);

		sample_put((struct sample *) wc[i].wr_id);
	}
}

void * ib_event_thread(void *n)
{
	struct infiniband *ib = (struct infiniband *) ((struct node *) n)->_vd;
	struct ibv_wc wc[ib->cq_size];
	int size;

	debug(LOG_IB | 1, "Initialized event based poll thread of node %s", node_name(n));

	while (1) {
		// Function blocks, until an event occurs
		ibv_get_cq_event(ib->ctx.comp_channel, &ib->ctx.send_cq, NULL);

		// Poll as long as WCs are available
		while ((size = ibv_poll_cq(ib->ctx.send_cq, ib->cq_size, wc)))
			ib->poll.on_compl(n, wc, &size);

		// Request a new event in the CQ and acknowledge event
		ibv_req_notify_cq(ib->ctx.send_cq, 0);
		ibv_ack_cq_events(ib->ctx.send_cq, 1);
	}
}

void * ib_busy_poll_thread(void *n)
{
	struct infiniband *ib = (struct infiniband *) ((struct node *) n)->_vd;
	struct ibv_wc wc[ib->cq_size];
	int size;

	debug(LOG_IB | 1, "Initialized busy poll thread of node %s", node_name(n));

	while (1) {
		// Poll as long as WCs are available
		while ((size = ibv_poll_cq(ib->ctx.send_cq, ib->cq_size, wc)))
			ib->poll.on_compl(n, wc, &size);

		if (ib->poll.stopThread)
			return NULL;
	}
}

static void ib_init_wc_poll(struct node *n)
{
	int ret;
	struct infiniband *ib = (struct infiniband *) n->_vd;
	ib->ctx.comp_channel = NULL;

	debug(LOG_IB | 1, "Starting to initialize completion queues and threads");

	if (ib->poll.poll_mode == EVENT) {
		// Create completion channel
		ib->ctx.comp_channel = ibv_create_comp_channel(ib->ctx.id->verbs);
		if (!ib->ctx.comp_channel)
			error("Could not create completion channel in node %s", node_name(n));

		debug(LOG_IB | 3, "Created Completion channel");
	}

	// Create completion queues and bind to channel (or NULL)
	ib->ctx.recv_cq = ibv_create_cq(ib->ctx.id->verbs,
					ib->cq_size,
					NULL,
					NULL,
					0);
	if (!ib->ctx.recv_cq)
		error("Could not create receive completion queue in node %s", node_name(n));

	debug(LOG_IB | 3, "Created receive Completion Queue");

	ib->ctx.send_cq = ibv_create_cq(ib->ctx.id->verbs,
					ib->cq_size,
					NULL,
					ib->ctx.comp_channel,
					0);
	if (!ib->ctx.send_cq)
		error("Could not create send completion queue in node %s", node_name(n));

	debug(LOG_IB | 3, "Created send Completion Queue");

	if (ib->poll.poll_mode == EVENT) {
		// Request notifications from completion queue
		ret = ibv_req_notify_cq(ib->ctx.send_cq, 0);
		if (ret)
			error("Failed to request notifiy CQ in node %s: %s",
				node_name(n), gai_strerror(ret));

		debug(LOG_IB | 3, "Called ibv_req_notificy_cq on send Completion Queue");
	}

	// Initialize polling pthread for source
	if (ib->is_source) {
		ret = pthread_create(&ib->poll.cq_poller_thread, NULL, ib->poll.poll_func, n);
		if (ret)
			error("Failed to create poll thread of node %s: %s",
				node_name(n), gai_strerror(ret));
	}
}

static void ib_build_ibv(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	int ret;

	debug(LOG_IB | 1, "Starting to build IBV components");

	//Allocate protection domain
	ib->ctx.pd = ibv_alloc_pd(ib->ctx.id->verbs);
	if (!ib->ctx.pd)
		error("Could not allocate protection domain in node %s", node_name(n));

	debug(LOG_IB | 3, "Allocated Protection Domain");

	// Initiate poll mode
	ib_init_wc_poll(n);

	// Prepare remaining Queue Pair (QP) attributes
	ib->qp_init.send_cq = ib->ctx.send_cq;
	ib->qp_init.recv_cq = ib->ctx.recv_cq;

	//ToDo: Set maximum inline data

	// Create the actual QP
	ret = rdma_create_qp(ib->ctx.id, ib->ctx.pd, &ib->qp_init);
	if (ret)
		error("Failed to create Queue Pair in node %s", node_name(n));

	debug(LOG_IB | 3, "Created Queue Pair with %i receive and %i send elements",
		ib->qp_init.cap.max_recv_wr, ib->qp_init.cap.max_send_wr);

	// Allocate memory
	ib->mem.p_recv.state = STATE_DESTROYED;
	ib->mem.p_recv.queue.state = STATE_DESTROYED;

	// Set pool size to maximum size of Receive Queue
	pool_init(&ib->mem.p_recv,
		ib->qp_init.cap.max_recv_wr,
		SAMPLE_DATA_LEN(DEFAULT_SAMPLELEN),
		&memory_type_heap);
	if (ret)
		error("Failed to init recv memory pool of node %s: %s",
			node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Created internal receive pool with %i elements",
			ib->qp_init.cap.max_recv_wr);

	//ToDo: initialize r_addr_key struct if mode is RDMA

	// Register memory for IB Device. Not necessary if data is send
	// exclusively inline
	ib->mem.mr_recv = ibv_reg_mr(
				ib->ctx.pd,
				(char*)&ib->mem.p_recv+ib->mem.p_recv.buffer_off,
				ib->mem.p_recv.len,
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	if (!ib->mem.mr_recv)
		error("Failed to register mr_recv with ibv_reg_mr of node %s",
			node_name(n));

	debug(LOG_IB | 3, "Registered receive pool with ibv_reg_mr");

	if (ib->is_source) {
		ib->mem.p_send.state = STATE_DESTROYED;
		ib->mem.p_send.queue.state = STATE_DESTROYED;

		// Set pool size to maximum size of Receive Queue
		pool_init(&ib->mem.p_send,
			ib->qp_init.cap.max_send_wr,
			sizeof(double),
			&memory_type_heap);
		if (ret)
			error("Failed to init send memory of node %s: %s",
				node_name(n), gai_strerror(ret));

		debug(LOG_IB | 3, "Created internal send pool with %i elements",
				ib->qp_init.cap.max_recv_wr);

		//ToDo: initialize r_addr_key struct if mode is RDMA

		// Register memory for IB Device. Not necessary if data is send
		// exclusively inline
		ib->mem.mr_send = ibv_reg_mr(
					ib->ctx.pd,
					(char*)&ib->mem.p_send+ib->mem.p_send.buffer_off,
					ib->mem.p_send.len,
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
		if (!ib->mem.mr_send)
			error("Failed to register mr_send with ibv_reg_mr of node %s",
				node_name(n));

		debug(LOG_IB | 3, "Registered send pool with ibv_reg_mr");
	}
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

static int ib_event(struct node *n, struct rdma_cm_event *event)
{
	int ret = 0;

	switch(event->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_ADDR_RESOLVED");
			ret = ib_addr_resolved(n);
			break;
		case RDMA_CM_EVENT_ADDR_ERROR:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_ADDR_ERROR");
			error("Address resolution (rdma_resolve_addr) failed!");
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_ROUTE_RESOLVED");
			ret = ib_route_resolved(n);
			break;
		case RDMA_CM_EVENT_ROUTE_ERROR:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_ROUTE_ERROR");
			error("Route resolution (rdma_resovle_route) failed!");
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_CONNECT_REQUEST");
			ret = ib_connect_request(n, event->id);
			break;
		case RDMA_CM_EVENT_CONNECT_ERROR:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_CONNECT_ERROR");
			error("An error has occurred trying to establish a connection!");
		case RDMA_CM_EVENT_REJECTED:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_REJECTED");
			error("Connection request or response was rejected by the remote end point!");
		case RDMA_CM_EVENT_ESTABLISHED:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_ESTABLISHED");
			info("Connection established in node %s", node_name(n));
			ret = 1;
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_DISCONNECTED");
			ret = ib_cleanup(n);
			break;
		default:
			error("Unknown event occurred: %u", event->event);
	}

	return ret;
}

int ib_reverse(struct node *n)
{
	return 0;
}

int ib_parse(struct node *n, json_t *cfg)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;

	int ret;
	char *local = NULL;
	char *remote = NULL;
	const char *port_space = "RDMA_PC_TCP";
	const char *poll_mode = "BUSY";
	const char *qp_type = "IBV_QPT_RC";
	int timeout = 1000;
	int cq_size = 128;
	int max_send_wr = 128;
	int max_recv_wr = 128;

	json_error_t err;
	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: i, \
					s?: s, s?: i, s?: s, s?: i, s?: i}",
		"remote", &remote,
		"local", &local,
		"rdma_port_space", &port_space,
		"resolution_timeout", &timeout,
		"poll_mode", &poll_mode,
		"cq_size", &cq_size,
		"qp_type", &qp_type,
		"max_send_wr", &max_send_wr,
		"max_recv_wr", &max_recv_wr
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	// Translate IP:PORT to a struct addrinfo
	char* ip_adr = strtok(local, ":");
	char* port = strtok(NULL, ":");

	ret = getaddrinfo(ip_adr, port, NULL, &ib->conn.src_addr);
	if (ret)
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), gai_strerror(ret));

	debug(LOG_IB | 4, "Translated %s:%s to a struct addrinfo in node %s", ip_adr, port, node_name(n));

	// Translate port space
	if (strcmp(port_space, "RDMA_PS_IPOIB") == 0)		ib->conn.port_space = RDMA_PS_IPOIB;
	else if (strcmp(port_space, "RDMA_PS_TCP") == 0) 	ib->conn.port_space = RDMA_PS_TCP;
	else if (strcmp(port_space, "RDMA_PS_UDP") == 0) 	ib->conn.port_space = RDMA_PS_UDP;
	else if (strcmp(port_space, "RDMA_PS_IB") == 0)  	ib->conn.port_space = RDMA_PS_IB;
	else
		error("Failed to translate rdma_port_space in node %s. %s is not a valid \
			port space supported by rdma_cma.h!", node_name(n), port_space);

	debug(LOG_IB | 4, "Translated %s to  enum rdma_port_space in node %s", port_space, node_name(n));

	// Set timeout
	ib->conn.timeout = timeout;

	debug(LOG_IB | 4, "Set  timeout to %i in node %s", timeout, node_name(n));

	// Translate poll mode
	if (strcmp(poll_mode, "EVENT") == 0) {
		ib->poll.poll_mode = EVENT;
		ib->poll.poll_func = ib_event_thread;
	}
	else if (strcmp(poll_mode, "BUSY") == 0) {
		ib->poll.poll_mode = BUSY;
		ib->poll.poll_func = ib_busy_poll_thread;
	}
	else
		error("Failed to translate poll_mode in node %s. %s is not a valid \
			poll mode!", node_name(n), poll_mode);

	debug(LOG_IB | 4, "Set poll mode to %s in node %s", poll_mode, node_name(n));

	// Set completion queue size
	ib->cq_size = cq_size;

	debug(LOG_IB | 4, "Set Completion Queue size to %i in node %s", cq_size, node_name(n));

	// Translate QP type
	if (strcmp(qp_type, "IBV_QPT_RC") == 0)		ib->qp_init.qp_type = IBV_QPT_RC;
	else if (strcmp(qp_type, "IBV_QPT_UC") == 0)	ib->qp_init.qp_type = IBV_QPT_UC;
	else if (strcmp(qp_type, "IBV_QPT_UD") == 0)	ib->qp_init.qp_type = IBV_QPT_UD;
	else
		error("Failed to translate qp_type in node %s. %s is not a valid \
			qp_type!", node_name(n), qp_type);

	debug(LOG_IB | 4, "Set Queue Pair type to %s in node %s", qp_type, node_name(n));

	// Set max. send and receive Work Requests
	ib->qp_init.cap.max_send_wr = max_send_wr;
	ib->qp_init.cap.max_recv_wr = max_recv_wr;

	debug(LOG_IB | 4, "Set max_send_wr and max_recv_wr in node %s to %i and %i, respectively",
			node_name(n), max_send_wr, max_recv_wr);

	// Set available receive Work Requests to 0
	ib->conn.available_recv_wrs = 0;

	// Set remaining QP attributes
	ib->qp_init.cap.max_send_sge = 1;
	ib->qp_init.cap.max_recv_sge = 1;

	//Check if node is a source and connect to target
	if (remote) {
		debug(LOG_IB | 3, "Node %s is set up to be able to send data (source and target)", node_name(n));

		ib->is_source = 1;

		// Translate address info
		char* ip_adr = strtok(remote, ":");
		char* port = strtok(NULL, ":");

		ret = getaddrinfo(ip_adr, port, NULL, &ib->conn.dst_addr);
		if (ret)
			error("Failed to resolve remote address '%s' of node %s: %s",
				remote, node_name(n), gai_strerror(ret));

		debug(LOG_IB | 4, "Translated %s:%s to a struct addrinfo in node %s", ip_adr, port, node_name(n));

		// Set correct Work Completion function
		ib->poll.on_compl = ib_completion_source;
	}
	else {
		debug(LOG_IB | 3, "Node %s is set up to be able to only receive data (target)", node_name(n));

		ib->is_source = 0;

		// Set correct Work Completion function
		ib->poll.on_compl = ib_completion_target;
	}

	return 0;
}

int ib_check(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;

	info("Starting check of node %s", node_name(n));

	// Check if the set value is a power of 2, and warn the user if this is not the case
	int max_send_pow = (int) pow(2, ceil(log2(ib->qp_init.cap.max_send_wr)));
	int max_recv_pow = (int) pow(2, ceil(log2(ib->qp_init.cap.max_recv_wr)));

	if (ib->qp_init.cap.max_send_wr != max_send_pow)
		warn("Max nr. of send WRs (%i) is not a power of 2! The HCA will change it to the next power of 2: %i",
			ib->qp_init.cap.max_send_wr, max_send_pow);

	if (ib->qp_init.cap.max_recv_wr != max_recv_pow)
		warn("Max nr. of recv WRs (%i) is not a power of 2! The HCA will change it to the next power of 2: %i",
			ib->qp_init.cap.max_recv_wr, max_recv_pow);


	// Check maximum size of max_recv_wr and max_send_wr
	if (ib->qp_init.cap.max_send_wr > 8192)
		warn("Max number of send WRs (%i) is bigger than send queue!", ib->qp_init.cap.max_send_wr);

	if (ib->qp_init.cap.max_recv_wr > 8192)
		warn("Max number of receive WRs (%i) is bigger than send queue!", ib->qp_init.cap.max_recv_wr);

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

void * ib_rdma_cm_event_thread(void *n)
{
	struct node *node = (struct node *) n;
	//struct infiniband *ib = (struct infiniband *) node->_vd;
	struct rdma_cm_event *event;

	debug(LOG_IB | 1, "Started rdma_cm_event thread of node %s", node_name(node));

	while (rdma_get_cm_event(rdma_event_channel, &event) == 0) {
		struct rdma_cm_event event_copy;

		memcpy(&event_copy, event, sizeof(*event));
		rdma_ack_cm_event(event);

		if (ib_event(n, &event_copy))
			break;
	}

	return NULL;
}

void * ib_disconnect_thread(void *n)
{
	struct node *node = (struct node *) n;
	struct infiniband *ib = (struct infiniband *) node->_vd;
	struct rdma_cm_event *event;

	debug(LOG_IB | 1, "Started disconnect thread of node %s", node_name(node));

	while (rdma_get_cm_event(rdma_event_channel, &event) == 0) {
		if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
			debug(LOG_IB | 2, "Received RDMA_CM_EVENT_DISCONNECTED");

			rdma_ack_cm_event(event);
			ib->conn.rdma_disconnect_called = 1;

			node_stop(node);
			return NULL;
		}
	}
	return NULL;
}

int ib_start(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct rdma_cm_event *event = NULL;
	int ret;

	debug(LOG_IB | 1, "Started ib_start");

	// Create event channel
	rdma_event_channel = rdma_create_event_channel();
	if (!rdma_event_channel)
		error("Failed to create event channel in node %s!", node_name(n));

	debug(LOG_IB | 3, "Created event channel");

	ret = rdma_create_id(rdma_event_channel, &ib->ctx.id, NULL, ib->conn.port_space);
	if (ret)
		error("Failed to create rdma_cm_id of node %s: %s",
			node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Created rdma_cm_id");

	// Bind rdma_cm_id to the HCA
	ret = rdma_bind_addr(ib->ctx.id, ib->conn.src_addr->ai_addr);
	if (ret)
		error("Failed to bind to local device of node %s: %s",
			node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Bound rdma_cm_id to Infiniband device");

	if (ib->is_source) {
		// Resolve address
		ret = rdma_resolve_addr(ib->ctx.id,
					NULL,
					ib->conn.dst_addr->ai_addr,
					ib->conn.timeout);
		if (ret)
			error("Failed to resolve remote address after %ims of node %s: %s",
				ib->conn.timeout, node_name(n), gai_strerror(ret));
	}
	else {
		// The ID will be overwritten for the target. If the event type is
		// RDMA_CM_EVENT_CONNECT_REQUEST, >then this references a new id for
		// that communication.
		ib->ctx.listen_id = ib->ctx.id;

		// Listen on rdma_cm_id for events
		ret = rdma_listen(ib->ctx.listen_id, 10);
		if (ret)
			error("Failed to listen to rdma_cm_id on node %s", node_name(n));

		debug(LOG_IB | 3, "Started to listen to rdma_cm_id");
	}

	// Several events should occur on the event channel, to make
	// sure the nodes are succesfully connected.
	debug(LOG_IB | 1, "Starting to monitor events on rdma_cm_id");

	//Create thread to monitor rdma_cm_event channel
	ret = pthread_create(&ib->conn.stop_thread, NULL, ib_disconnect_thread, n);
	if (ret)
		error("Failed to create thread to monitor disconnects in node %s: %s",
			node_name(n), gai_strerror(ret));


	//Wait until rdma_cm_event thread sets state to STARTED
	while ( !(n->state == STATE_CONNECTED));

	return 0;
}

int ib_stop(struct node *n)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct rdma_cm_event *event = NULL;
	int ret;

	// Call RDMA disconnect function
	// Will flush all outstanding WRs to the Completion Queue and
	// will call RDMA_CM_EVENT_DISCONNECTED if that is done.
	ret = rdma_disconnect(ib->ctx.id);
	if (ret)
		error("Error while calling rdma_disconnect in node %s: %s",
		    node_name(n), gai_strerror(ret));

	debug(LOG_IB | 3, "Called rdma_disconnect");

	// If disconnected event already occured, directly call cleanup function
	if (ib->conn.rdma_disconnect_called)
		ib_cleanup(n);
	else {
		// Else, wait for event to occur
		ib->conn.rdma_disconnect_called = 1;
		rdma_get_cm_event(rdma_event_channel, &event);

		rdma_ack_cm_event(event);

		ib_event(n, event);
	}

	return 0;
}

int ib_init(struct super_node *n)
{
	return 0;
}

int ib_deinit()
{
	return 0;
}

int ib_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_wc wc[n->in.vectorize];
	struct ibv_recv_wr wr[cnt], *bad_wr = NULL;
    	struct ibv_sge sge[cnt];
	struct ibv_mr *mr;
	int ret;

	debug(LOG_IB | 15, "ib_read is called");

	if (ib->conn.available_recv_wrs < ib->qp_init.cap.max_recv_wr && cnt==n->in.vectorize)	{
		// Get Memory Region
		mr = memory_ib_get_mr(smps[0]);

		for (int i = 0; i < cnt; i++) {
			// Increase refcnt of sample
			sample_get(smps[i]);

			// Prepare receive Scatter/Gather element
    			sge[i].addr = (uint64_t) &smps[i]->data;
    			sge[i].length = SAMPLE_DATA_LEN(DEFAULT_SAMPLELEN);
    			sge[i].lkey = mr->lkey;

    			// Prepare a receive Work Request
    			wr[i].wr_id = (uintptr_t) smps[i];
			wr[i].next = &wr[i+1];
    			wr[i].sg_list = &sge[i];
    			wr[i].num_sge = 1;

			ib->conn.available_recv_wrs++;

			if (ib->conn.available_recv_wrs == ib->qp_init.cap.max_recv_wr || i==(cnt-1)) {
				debug(LOG_IB | 10, "Prepared %i new receive Work Requests", (i+1));

				wr[i].next = NULL;
				break;
			}
		}

    		// Post list of Work Requests
    		ret = ibv_post_recv(ib->ctx.id->qp, &wr[0], &bad_wr);
		if (ret)
			error("Was unable to post receive WR in node %s: %i, bad WR ID: 0x%lx",
			    node_name(n), ret, bad_wr->wr_id);

		debug(LOG_IB | 10, "Succesfully posted receive Work Requests");

	}

	// Poll Completion Queue
	ret = ibv_poll_cq(ib->ctx.recv_cq, n->in.vectorize, wc);

	if (ret) {
		debug(LOG_IB | 10, "Received %i Work Completions", ret);

		ib->conn.available_recv_wrs -= ret;

		for (int i = 0; i < ret; i++) {
			if (wc[i].status == IBV_WC_WR_FLUSH_ERR) {
				debug(LOG_IB | 5, "Received IBV_WC_WR_FLUSH_ERR in ib_read. Ignore it.");

				ret = 0;
			}
			else if (wc[i].status != IBV_WC_SUCCESS) {
				warn("Work Completion status was not IBV_WC_SUCCES in node %s: %i",
					node_name(n), wc[i].status);
				ret = 0;
			}
			else if (wc[i].opcode & IBV_WC_RECV) {
				smps[i] = (struct sample*)(wc[i].wr_id);
				smps[i]->length = wc[i].byte_len/sizeof(double);
			}
			else
				ret = 0;

			//Release sample
			sample_put((struct sample *) (wc[i].wr_id));
			debug(LOG_IB | 10, "Releasing sample %p", (struct sample *) (wc[i].wr_id));
		}
	}

	return ret;
}

int ib_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct infiniband *ib = (struct infiniband *) n->_vd;
	struct ibv_send_wr wr[cnt], *bad_wr = NULL;
	struct ibv_sge sge[cnt];
	struct ibv_mr *mr;
	int ret;

	debug(LOG_IB | 10, "ib_write is called");

	memset(&wr, 0, sizeof(wr));

	//ToDo: Place this into configuration and create checks if settings are valid
	int send_inline = 1;

	debug(LOG_IB | 10, "Data will be send inline [0/1]: %i", send_inline);

	// Get Memory Region
	mr = memory_ib_get_mr(smps[0]);

	for (int i = 0; i < cnt; i++) {
		// Increase refcnt of sample
		sample_get(smps[i]);

		//Set Scatter/Gather element to data of sample
		sge[i].addr = (uint64_t)&smps[i]->data;
		sge[i].length = smps[i]->length*sizeof(double);
		sge[i].lkey = mr->lkey;

		// Set Send Work Request
		wr[i].wr_id = (uintptr_t)smps[i]; //This way the sample can be release in WC
		wr[i].sg_list = &sge[i];
		wr[i].num_sge = 1;

		if (i == (cnt-1)) {
			debug(LOG_IB | 10, "Prepared %i send Work Requests", (i+1));
			wr[i].next = NULL;
		}
		else
			wr[i].next = &wr[i+1];

		wr[i].send_flags = IBV_SEND_SIGNALED | (send_inline << 3);
		wr[i].imm_data = htonl(0); //ToDo: set this to a useful value
		wr[i].opcode = IBV_WR_SEND_WITH_IMM;
	}

	//Send linked list of Work Requests
	ret = ibv_post_send(ib->ctx.id->qp, wr, &bad_wr);
	if (ret) {
		error("Failed to send message in node %s: %i, bad WR ID: 0x%lx",
			node_name(n), ret, bad_wr->wr_id);

		return -ret;
	}

	debug(LOG_IB | 4, "Succesfully posted receive Work Requests");

	return cnt;
}

int ib_fd(struct node *n)
{
	return 0;
}

static struct plugin p = {
	.name           = "infiniband",
	.description    = "Infiniband",
	.type           = PLUGIN_TYPE_NODE,
	.node           = {
		.vectorize      = 0,
		.size           = sizeof(struct infiniband),
		.reverse        = ib_reverse,
		.parse          = ib_parse,
		.check		= ib_check,
		.print          = ib_print,
		.start          = ib_start,
		.destroy        = ib_destroy,
		.stop           = ib_stop,
		.init           = ib_init,
		.deinit         = ib_deinit,
		.read           = ib_read,
		.write          = ib_write,
		.fd             = ib_fd,
		.memory_type    = memory_ib
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
