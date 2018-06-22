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
#include <rdma/rdma_cma.h>

static int infiniband_addr_resolved(struct rdma_cm_id *id)
{
    return 0;
}

static int infiniband_route_resolved(struct rdma_cm_id *id)
{
    return 0;
}

static int infiniband_connect_request(struct rdma_cm_id *id)
{
    return 0;
}

static int infiniband_event(struct rdma_cm_event *event)
{
    int ret = 0;

    switch(event->event)
    {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
            ret = infiniband_addr_resolved(event->id);
            break;
        case RDMA_CM_EVENT_ADDR_ERROR:
            error("Address resolution (rdma_resolve_addr) failed!");
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            ret = infiniband_route_resolved(event->id);
            break;
        case RDMA_CM_EVENT_ROUTE_ERROR:
            error("Route resolution (rdma_resovle_route) failed!");
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            break;
        case RDMA_CM_EVENT_CONNECT_ERROR:
            error("An error has occurred trying to establish a connection!");
        case RDMA_CM_EVENT_REJECTED:
            error("Connection request or response was rejected by the remote end point!");
        case RDMA_CM_EVENT_ESTABLISHED:
            ret = 1;
            break;
        default:
            error("Unknown event occurred: %u",
                event->event);
    }

    return ret;
}

int infiniband_reverse(struct node *n)
{
    return 0;
}

int infiniband_parse(struct node *n, json_t *cfg)
{
    struct infiniband *ib = (struct infiniband *) n->_vd;

    int ret;
    const char *local = NULL;
    const char *remote = NULL;
    const char *port_space = NULL;
    const int timeout;

    json_error_t err;
    ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: i}",
        "remote", &remote,
        "local", &local,
        "rdma_port_space", &port_space,
        "resolution_timeout", &timeout
    );
    if(ret)
        jerror(&err, "Failed to parse configuration of node %s", node_name(n));

    // Translate IP:PORT to a struct addrinfo
    ret = getaddrinfo(local, NULL, NULL, &ib->conn.src_addr);
    if(ret) {
        error("Failed to resolve local address '%s' of node %s: %s",
            local, node_name(n), gai_strerror(ret));
    }

    // Translate port space and create rdma_cm_id object
    if(strcmp(port_space, "RDMA_PS_IPOIB") == 0)    ib->conn.port_space = RDMA_PS_IPOIB;
    else if(strcmp(port_space, "RDMA_PS_TCP") == 0) ib->conn.port_space = RDMA_PS_TCP;
    else if(strcmp(port_space, "RDMA_PS_UDP") == 0) ib->conn.port_space = RDMA_PS_UDP;
    else if(strcmp(port_space, "RDMA_PS_IB") == 0)  ib->conn.port_space = RDMA_PS_IB;
    else {
        error("Failed to translate rdma_port_space in node %s. %s is not a valid \
            port space supported by rdma_cma.h!", node_name(n), port_space);
    }

    //Check if node is a source and connect to target
    if(remote)
    {
        ib->is_source = 1;

        // Translate address info
        ret = getaddrinfo(remote, NULL, NULL, &ib->conn.dst_addr);
        if(ret) {
            error("Failed to resolve remote address '%s' of node %s: %s",
                remote, node_name(n), gai_strerror(ret));
        }

    }
    else
        ib->is_source = 0;

    return 0;
}

char * infiniband_print(struct node *n)
{
    return 0;
}

int infiniband_destroy(struct node *n)
{
    return 0;
}

int infiniband_start(struct node *n)
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
    info("Succesfully created CM RDMA ID of node %s",
            node_name(n));

    // Bind rdma_cm_id to the HCA
    ret = rdma_bind_addr(ib->id, ib->conn.src_addr->ai_addr);
    if(ret) {
        error("Failed to bind to local device of node %s: %s",
            node_name(n), gai_strerror(ret));
    }
    info("Bound to Infiniband device of node %s",
            node_name(n));
    
    if(ib->is_source)
    {
        // Resolve address
        ret = rdma_resolve_addr(ib->id, NULL, ib->conn.dst_addr->ai_addr, ib->conn.timeout); 
        if(ret) {
            error("Failed to resolve remote address after %ims of node %s: %s",
                ib->conn.timeout, node_name(n), gai_strerror(ret));
        }

    }

    // Several events should occur on the event channel, to make
    // sure the nodes are succesfully connected.
    info("Starting to monitor events on rdma_cm_id.\n");
    while(rdma_get_cm_event(ib->ec, &event) == 0)
    {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));

        if(infiniband_event(&event_copy))
            break;
    }

    return 0;
}

int infiniband_stop(struct node *n)
{
    return 0;
}

int infiniband_init(struct super_node *n)
{
    
    return 0;
}

int infiniband_deinit()
{
    return 0;
}

int infiniband_read(struct node *n, struct sample *smps[], unsigned cnt)
{
    return 0;
}

int infiniband_write(struct node *n, struct sample *smps[], unsigned cnt)
{
    return 0;
}

int infiniband_fd(struct node *n)
{
    return 0;
}

static struct plugin p = {
    .name       = "infiniband",
    .description    = "Infiniband)",
    .type       = PLUGIN_TYPE_NODE,
    .node       = {
        .vectorize  = 0,
        .size       = sizeof(struct infiniband),
        .reverse    = infiniband_reverse,
        .parse      = infiniband_parse,
        .print      = infiniband_print,
        .start      = infiniband_start,
        .destroy    = infiniband_destroy,
        .stop       = infiniband_stop,
        .init       = infiniband_init,
        .deinit     = infiniband_deinit,
        .read       = infiniband_read,
        .write      = infiniband_write,
        .fd         = infiniband_fd
    }
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
