/** Node-type: CAN bus
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
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

/**
 * @addtogroup can CAN bus Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <jansson.h>

#include <villas/timing.h>

/* Forward declarations */
struct node;
union signal_data;

struct can_signal {
    uint32_t id;
    int offset;
    int size;
};

struct can {
    /* Settings */
    char *interface_name;
    double *sample_rate;
    struct can_signal *in;
    struct can_signal *out;

    /* States */
    int socket;
    union signal_data *sample_buf;
    size_t sample_buf_num;
    struct timespec start_time;
};

/** @see node_type::init */
int can_init(struct node *n);

/** @see node_type::destroy */
int can_destroy(struct node *n);

/** @see node_type::parse */
int can_parse(struct node *n, json_t *cfg);

/** @see node_type::print */
char * can_print(struct node *n);

/** @see node_type::check */
int can_check();

/** @see node_type::prepare */
int can_prepare();

/** @see node_type::start */
int can_start(struct node *n);

/** @see node_type::stop */
int can_stop(struct node *n);

/** @see node_type::write */
int can_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::read */
int can_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::poll_fds */
int can_poll_fds(struct node *n, int fds[]);

/** @} */
