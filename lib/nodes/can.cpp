/** Node-type: CAN bus
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <villas/nodes/can.hpp>
#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/super_node.hpp>

/* Forward declartions */
static struct plugin p;

using namespace villas::node;
using namespace villas::utils;

int can_type_start(villas::node::SuperNode *sn)
{
	/* TODO: Add implementation here */

	return 0;
}

int can_type_stop()
{
	/* TODO: Add implementation here */

	return 0;
}

int can_init(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	c->interface_name = nullptr;
	c->socket = 0;

	return 0;
}

int can_destroy(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

    free(c->interface_name);
    if (c->socket != 0) {
        close(c->socket);
    }
	return 0;
}

int can_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct can *c = (struct can *) n->_vd;

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s }",
		"interface_name", &c->interface_name
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * can_print(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	return strf("interface_name=%s", c->interface_name);
}

int can_check(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	if (c->interface_name != nullptr && strlen(c->interface_name) > 0)
		return -1;

	return 0;
}

int can_prepare(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */
//
//	c->state1 = c->setting1;
//
//	if (strcmp(c->setting2, "double") == 0)
//		c->state1 *= 2;

	return 0;
}

int can_start(struct node *n)
{
	struct sockaddr_can addr = {0};
	struct can_frame frame;
	struct ifreq ifr;

	struct can *c = (struct can *) n->_vd;
	c->start_time = time_now();

	if((c->socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
	    return strf("Error while opening CAN socket");
	}

	strcpy(ifr.ifr_name, c->interface_name);
    if (ioctl(c->socket, SIOCGIFINDEX, &ifr) != 0) {
        return strf("Could not find interface with name \"%s\".", c->interface_name);
    }

	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	if(bind(c->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return strf("Could not bind to interface with name \"%s\" (%d).", c->interface_name, ifr.ifr_ifindex);
	}

	return 0;
}

int can_stop(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

    if (c->socket != 0) {
        close(c->socket);
        c->socket = 0;
    }
    //TODO: do we need to free c->interface_name here?

	return 0;
}

int can_pause(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_resume(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int nbytes;
	struct can_frame frame;
	struct timespec now;

	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

    nbytes = read(c->socket, &frame, sizeof(struct can_frame));
    if (nbytes != sizeof(struct can_frame)) {
        return strf("CAN read() error. read() returned %d bytes but expected %d",
                    nbytes, sizeof(struct can_frame));
    }

    printf("id:%d, len:%d, data: 0x%x:0x%x\n", frame.can_id,
            frame.can_dlc,
            ((uint32_t*)&frame.data)[0],
            ((uint32_t*)&frame.data)[1]);

	now = time_now();
	smps[0]->data[0].f = time_delta(&now, &c->start_time);

	return nbytes;
}

int can_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int nbytes;
	struct can_frame frame = {0};
    struct can *c = (struct can *) n->_vd;

    nbytes = write(c->socket, &frame, sizeof(struct can_frame));
    if (nbytes != sizeof(struct can_frame)) {
        return strf("CAN write() error. write() returned %d bytes but expected %d",
                    nbytes, sizeof(struct can_frame));
    }


	return nbytes;
}

int can_reverse(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_poll_fds(struct node *n, int fds[])
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

int can_netem_fds(struct node *n, int fds[])
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name			= "can";
	p.description		= "CAN bus for Xilinx MPSoC boards";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct can);
	p.node.type.start	= can_type_start;
	p.node.type.stop	= can_type_stop;
	p.node.init		= can_init;
	p.node.destroy		= can_destroy;
	p.node.prepare		= can_prepare;
	p.node.parse		= can_parse;
	p.node.print		= can_print;
	p.node.check		= can_check;
	p.node.start		= can_start;
	p.node.stop		= can_stop;
	p.node.pause		= can_pause;
	p.node.resume		= can_resume;
	p.node.read		= can_read;
	p.node.write		= can_write;
	p.node.reverse		= can_reverse;
	p.node.poll_fds		= can_poll_fds;
	p.node.netem_fds	= can_netem_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
