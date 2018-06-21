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

static void infiniband_log_cb(struct infiniband *ib, void *userdata, int level, const char *str)
{
}

static void infiniband_connect_cb(struct infiniband *ib, void *userdata, int result)
{
}

static void infiniband_disconnect_cb(struct infiniband *ib, void *userdata, int result)
{
}

static void infiniband_message_cb(struct infiniband *ib, void *userdata)
{
}

static void infiniband_subscribe_cb(struct infiniband *ib, void *userdata, int mid, int qos_count, const int *granted_qos)
{
}

int infiniband_reverse(struct node *n)
{
    return 0;
}

int infiniband_parse(struct node *n, json_t *cfg)
{
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
    return 0;
}

int infiniband_stop(struct node *n)
{
    return 0;
}

int infiniband_init()
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
