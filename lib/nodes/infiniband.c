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

#include <villas/nodes/infiniband.h>
#include <villas/plugin.h>
#include <villas/utils.h>
#include <villas/format_type.h>
#include <villas/memory.h>
#include <villas/pool.h>

#include <rdma/rdma_cma.h>

int ib_post_recv_wrs(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    struct ibv_recv_wr wr, *bad_wr = NULL;
    int ret;
    struct ibv_sge sge;

    // Prepare receive Scatter/Gather element
    sge.addr = (uintptr_t)pool_get(&ib->mem.p_recv);
    sge.length = ib->mem.p_recv.blocksz;
    sge.lkey = ib->mem.mr_recv->lkey;

    // Prepare a receive Work Request
    wr.wr_id = (uintptr_t)sge.addr;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    // Post Work Request
    ret = ibv_post_recv(ib->id->qp, &wr, &bad_wr);

    return ret; 
}

static void ib_create_busy_poll(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    
    // Create completion queue and bind to channel
    ib->ctx.cq = ibv_create_cq(ib->id->verbs, ib->cq_size, NULL, NULL, 0);
    if(!ib->ctx.cq)
        error("Could not create completion queue in node %s.", node_name(n));

    //ToDo: Create poll pthread
}

static void ib_create_event(struct node *n)
{
    int ret;
    struct infiniband *ib = (struct infiniband *) n->_vd;

    // Create completion channel
    ib->ctx.comp_channel = ibv_create_comp_channel(ib->id->verbs);
    if(!ib->ctx.comp_channel)
        error("Could not create completion channel in node %s.", node_name(n));

    // Create completion queue and bind to channel
    ib->ctx.cq = ibv_create_cq(ib->id->verbs, 
                               ib->cq_size, 
                               NULL, 
                               ib->ctx.comp_channel, 
                               0);
    if(!ib->ctx.cq)
        error("Could not create completion queue in node %s.", node_name(n));

    // Request notifications from completion queue
    ret = ibv_req_notify_cq(ib->ctx.cq, 0);
    if(ret)
        error("Failed to request notifiy CQ in node %s: %s",
            node_name(n), gai_strerror(ret));

    //ToDo: Create poll pthread
}

static void ib_build_ibv(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;
    
    //Allocate protection domain
    ib->ctx.pd = ibv_alloc_pd(ib->id->verbs);
    if(!ib->ctx.pd)
        error("Could not allocate protection domain in node %s.", node_name(n));

    // Initiate poll mode
    switch(ib->poll.poll_mode)
    {
        case EVENT:
            ib_create_event(n);
            break;
        case BUSY:
            ib_create_busy_poll(n);
            break;
    }

    // Prepare remaining Queue Pair (QP) attributes
    ib->qp_init.send_cq = ib->ctx.cq;
    ib->qp_init.recv_cq = ib->ctx.cq;

    //ToDo: Set maximum inline data

    // Create the actual QP
    ret = rdma_create_qp(ib->id, ib->ctx.pd, &ib->qp_init);
    if(ret) 
        error("Failed to create Queue Pair in node %s.", node_name(n));

    info("Created Queue Pair.");

    // Allocate memory
    ib->mem.p_recv.state = STATE_DESTROYED;
    ib->mem.p_recv.queue.state = STATE_DESTROYED;

    // Set pool size to maximum size of Receive Queue
    pool_init(&ib->mem.p_recv, 
            ib->qp_init.cap.max_recv_wr, 
            sizeof(double), 
            &memtype_heap);
    if(ret) {
        error("Failed to init recv memory pool of node %s: %s",
            node_name(n), gai_strerror(ret));
    }

    //ToDo: initialize r_addr_key struct if mode is RDMA
    
    // Register memory for IB Device. Not necessary if data is send
    // exclusively inline
    ib->mem.mr_recv = ibv_reg_mr(
            ib->ctx.pd,
            (char*)&ib->mem.p_recv+ib->mem.p_recv.buffer_off,
            ib->mem.p_recv.len,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if(!ib->mem.mr_recv) {
        error("Failed to register mr_recv with ibv_reg_mr of node %s.",
            node_name(n));
    }
    info("Allocated receive memory.");

    if(ib->is_source)
    {
        ib->mem.p_send.state = STATE_DESTROYED;
        ib->mem.p_send.queue.state = STATE_DESTROYED;

        // Set pool size to maximum size of Receive Queue
        pool_init(&ib->mem.p_send, 
                ib->qp_init.cap.max_send_wr, 
                sizeof(double), 
                &memtype_heap);
        if(ret) {
            error("Failed to init send memory of node %s: %s",
                node_name(n), gai_strerror(ret));
        }

        //ToDo: initialize r_addr_key struct if mode is RDMA
        
        // Register memory for IB Device. Not necessary if data is send
        // exclusively inline
        ib->mem.mr_send = ibv_reg_mr(
                ib->ctx.pd,
                (char*)&ib->mem.p_send+ib->mem.p_send.buffer_off,
                ib->mem.p_send.len,
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        if(!ib->mem.mr_send) {
            error("Failed to register mr_send with ibv_reg_mr of node %s.",
                node_name(n));
        }
        info("Allocated send memory.");

    }
    
    // Post Receive Work Requests to be able to receive data
    // Fill complete Receive Queue during initialization
    for(int i=0; i<ib->qp_init.cap.max_recv_wr; i++)
    {
        ret = ib_post_recv_wrs(n);
        if(ret) {
            error("Failed to post initial receive Work Requests of node %s.",
                node_name(n));
        }
    }
    info("Filled the complete Receive Queue.");
}

static int ib_addr_resolved(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;

    info("Successfully resolved address.");

    // Build all components from IB Verbs
    ib_build_ibv(n);

    // Resolve address
    ret = rdma_resolve_route(ib->id, ib->conn.timeout);
    if(ret) 
        error("Failed to resolve route in node %s.", node_name(n));

    //ToDo: create check if data can be send inline

    return 0;
}

static int ib_route_resolved(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;

    info("Successfully resolved route.");

    //ToDo: Post receive WRs
    
    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));

    // Send connection request
    ret = rdma_connect(ib->id, &cm_params);
    if(ret) 
        error("Failed to connect in node %s.", node_name(n));

    info("Called rdma_connect.");
    
    return 0;
}

static int ib_connect_request(struct node *n, struct rdma_cm_id *id)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;
    info("Received a connection request!");

    ib->id = id;
    ib_build_ibv(n);
    
    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));

    // Accept connection request
    ret = rdma_accept(ib->id, &cm_params);
    if(ret) 
        error("Failed to connect in node %s.", node_name(n));

    info("Successfully accepted connection request.");
    
    return 0;
}

static int ib_event(struct node *n, struct rdma_cm_event *event)
{
    int ret = 0;

    switch(event->event)
    {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
            ret = ib_addr_resolved(n);
            break;
        case RDMA_CM_EVENT_ADDR_ERROR:
            error("Address resolution (rdma_resolve_addr) failed!");
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            ret = ib_route_resolved(n);
            break;
        case RDMA_CM_EVENT_ROUTE_ERROR:
            error("Route resolution (rdma_resovle_route) failed!");
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            ret = ib_connect_request(n, event->id);
            break;
        case RDMA_CM_EVENT_CONNECT_ERROR:
            error("An error has occurred trying to establish a connection!");
        case RDMA_CM_EVENT_REJECTED:
            error("Connection request or response was rejected by the remote end point!");
        case RDMA_CM_EVENT_ESTABLISHED:
            info("Connection established!");
            ret = 1;
            break;
        default:
            error("Unknown event occurred: %u",
                event->event);
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
    const char *local = NULL;
    const char *remote = NULL;
    const char *port_space = "RDMA_PC_TCP";
    const char *poll_mode = "BUSY";
    const char *qp_type = "IBV_QPT_RC";
    int timeout = 1000;
    int cq_size = 10;
    int max_send_wr = 100;
    int max_recv_wr = 100;

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
    if(ret)
        jerror(&err, "Failed to parse configuration of node %s", node_name(n));

    // Translate IP:PORT to a struct addrinfo
    //ToDo: Fix fixed port
    ret = getaddrinfo(local, (char *)"13337", NULL, &ib->conn.src_addr);
    if(ret) {
        error("Failed to resolve local address '%s' of node %s: %s",
            local, node_name(n), gai_strerror(ret));
    }

    // Translate port space
    if(strcmp(port_space, "RDMA_PS_IPOIB") == 0)    ib->conn.port_space = RDMA_PS_IPOIB;
    else if(strcmp(port_space, "RDMA_PS_TCP") == 0) ib->conn.port_space = RDMA_PS_TCP;
    else if(strcmp(port_space, "RDMA_PS_UDP") == 0) ib->conn.port_space = RDMA_PS_UDP;
    else if(strcmp(port_space, "RDMA_PS_IB") == 0)  ib->conn.port_space = RDMA_PS_IB;
    else {
        error("Failed to translate rdma_port_space in node %s. %s is not a valid \
            port space supported by rdma_cma.h!", node_name(n), port_space);
    }

    // Set timeout
    ib->conn.timeout = timeout;
    
    // Translate poll mode
    if(strcmp(poll_mode, "EVENT") == 0)    ib->poll.poll_mode = EVENT;
    else if(strcmp(poll_mode, "BUSY") == 0) ib->poll.poll_mode = BUSY;
    else {
        error("Failed to translate poll_mode in node %s. %s is not a valid \
            poll mode!", node_name(n), poll_mode);
    }

    // Set completion queue size
    ib->cq_size = cq_size;

    // Translate QP type
    if(strcmp(qp_type, "IBV_QPT_RC") == 0)    ib->qp_init.qp_type = IBV_QPT_RC;
    else if(strcmp(qp_type, "IBV_QPT_UC") == 0) ib->qp_init.qp_type = IBV_QPT_UC;
    else if(strcmp(qp_type, "IBV_QPT_UD") == 0) ib->qp_init.qp_type = IBV_QPT_UD;
    else {
        error("Failed to translate qp_type in node %s. %s is not a valid \
            qp_type!", node_name(n), qp_type);
    }

    // Set max. send and receive Work Requests
    ib->qp_init.cap.max_send_wr = max_send_wr;
    ib->qp_init.cap.max_recv_wr = max_recv_wr;

    // Set remaining QP attributes
    ib->qp_init.cap.max_send_sge = 1;
    ib->qp_init.cap.max_recv_sge = 1;

    //Check if node is a source and connect to target
    if(remote)
    {
        ib->is_source = 1;

        // Translate address info
        //ToDo: Fix fixed port
        ret = getaddrinfo(remote, (char *)"13337", NULL, &ib->conn.dst_addr);
        if(ret) {
            error("Failed to resolve remote address '%s' of node %s: %s",
                remote, node_name(n), gai_strerror(ret));
        }
    }
    else
        ib->is_source = 0;

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

int ib_start(struct node *n)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;
    struct rdma_cm_event *event = NULL;
    int ret;

    // Create event channel
    ib->ec = rdma_create_event_channel();
    if(!ib->ec) {
        error("Failed to create event channel in node %s!",
            node_name(n));
    }

    ret = rdma_create_id(ib->ec, &ib->id, NULL, ib->conn.port_space);
    if(ret) {
        error("Failed to create rdma_cm_id of node %s: %s",
            node_name(n), gai_strerror(ret));
    }
    info("Succesfully created rdma_cm_id.");

    // Bind rdma_cm_id to the HCA
    ret = rdma_bind_addr(ib->id, ib->conn.src_addr->ai_addr);
    if(ret) {
        error("Failed to bind to local device of node %s: %s",
            node_name(n), gai_strerror(ret));
    }
    info("Bound rdma_cm_id to Infiniband device.");
    
    if(ib->is_source)
    {
        // Resolve address
        ret = rdma_resolve_addr(ib->id, 
                                NULL, 
                                ib->conn.dst_addr->ai_addr, 
                                ib->conn.timeout); 
        if(ret) {
            error("Failed to resolve remote address after %ims of node %s: %s",
                ib->conn.timeout, node_name(n), gai_strerror(ret));
        }
    }
    else
    {
        // The ID will be overwritten for the target. If the event type is 
        // RDMA_CM_EVENT_CONNECT_REQUEST, >then this references a new id for 
        // that communication. 
        ib->listen_id = ib->id;

        // Listen on rdma_cm_id for events
        ret = rdma_listen(ib->listen_id, 10);
        if(ret) {
            error("Failed to listen to rdma_cm_id on node %s", node_name(n));
        }
    }

    // Several events should occur on the event channel, to make
    // sure the nodes are succesfully connected.
    info("Starting to monitor events on rdma_cm_id.");

    while(rdma_get_cm_event(ib->ec, &event) == 0)
    {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));

        rdma_ack_cm_event(event);

        if(ib_event(n, &event_copy))
            break;
    }

    return 0;
}

int ib_stop(struct node *n)
{
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
    //Create separate thread for polling! This impelemtation is just
    //for testing purposes
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;
    struct ibv_wc wc[cnt];
	union {
		double  f;
		int64_t i;
	} *data;	  

    ret = ibv_poll_cq(ib->ctx.cq, cnt, wc);

    if(ret)
    {
        data = malloc(ret*sizeof(double));

        for(int i=0; i<ret; i++)
        {
            if(wc[i].status != IBV_WC_SUCCESS)
                error("Work Completion status was not IBV_WC_SUCCES in node %s", node_name(n));

            data[i].f = *(double*)(wc[i].wr_id);
        }
        smps[0]->length = ret;
        smps[0]->capacity = cnt;
        memcpy(smps[0]->data, data, ret*sizeof(double));
    }

    return ret;
}

int ib_write(struct node *n, struct sample *smps[], unsigned cnt)
{
    /* Send pool is not used at this moment! */
    struct infiniband *ib = (struct infiniband *) n->_vd;
    int ret;
    struct ibv_send_wr wr[smps[0]->length], *bad_wr = NULL;
    struct ibv_sge sge[smps[0]->length];

    memset(&wr, 0, sizeof(wr));

    //ToDo: Place this into configuration and create checks if settings are valid
    int send_inline = 1;

    for(int i=0; i<smps[0]->length; i++)
    {
        // If data is send inline, it is not necessary to copy data to protected
        // memory region first.
        if(send_inline)
        {
            sge[i].addr = (uint64_t)&smps[0]->data[i].f;
            sge[i].length = sizeof(double);
        }
        else
        {
            //- copy value to send_region
            //- give pointer to start of array
        }

        // Set Send Work Request
        wr[i].wr_id = 0; //ToDo: set this to a useful value
        wr[i].sg_list = &sge[i];
        wr[i].num_sge = 1; //ToDo: Right now only smps[0] is sg_list. This can be extended
                           //furthermore we should break the transaction up if inline mode
                           //is selected
                           
        if(i == (smps[0]->length-1))
            wr[i].next = NULL;
        else
            wr[i].next = &wr[i+1];
        wr[i].send_flags = IBV_SEND_SIGNALED | (send_inline<<3);
        wr[i].imm_data = htonl(0); //ToDo: set this to a useful value
        wr[i].opcode = IBV_WR_SEND_WITH_IMM;

    }
    
    //Send linked list of Work Requests
    ret = ibv_post_send(ib->id->qp, wr, &bad_wr);
    if(ret)
    {
        error("Failed to send message in node %s: %s",
            node_name(n), gai_strerror(ret));

        return -ret;
    }
    
    /* Debugging */
    struct ibv_wc wc[5];
    int size;
    while(1)
    {
        size = ibv_poll_cq(ib->ctx.cq, 5, wc);
        if(size)
            for(int j=0; j<size; j++)
                printf("Error: %i\n", wc[j].status);
    }

    return cnt;
}

int ib_fd(struct node *n)
{
    return 0;
}

static struct plugin p = {
    .name       = "infiniband",
    .description    = "Infiniband",
    .type       = PLUGIN_TYPE_NODE,
    .node       = {
        .vectorize  = 0,
        .size       = sizeof(struct infiniband),
        .reverse    = ib_reverse,
        .parse      = ib_parse,
        .print      = ib_print,
        .start      = ib_start,
        .destroy    = ib_destroy,
        .stop       = ib_stop,
        .init       = ib_init,
        .deinit     = ib_deinit,
        .read       = ib_read,
        .write      = ib_write,
        .fd         = ib_fd
    }
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
