/** Node C-API
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <stdbool.h>

#include <uuid/uuid.h>
#include <jansson.h>

typedef void *vnode;
typedef void *vsample;

vnode * node_new(const char *json_str, const char *sn_uuid_str);

int node_prepare(vnode *n);

int node_check(vnode *n);

int node_start(vnode *n);

int node_stop(vnode *n);

int node_pause(vnode *n);

int node_resume(vnode *n);

int node_restart(vnode *n);

int node_destroy(vnode *n);

const char * node_name(vnode *n);

const char * node_name_short(vnode *n);

const char * node_name_full(vnode *n);

const char * node_details(vnode *n);

unsigned node_input_signals_max_cnt(vnode *n);

unsigned node_output_signals_max_cnt(vnode *n);

int node_reverse(vnode *n);

int node_read(vnode *n, vsample **smps, unsigned cnt);

int node_write(vnode *n, vsample **smps, unsigned cnt);

int node_poll_fds(vnode *n, int fds[]);

int node_netem_fds(vnode *n, int fds[]);

bool node_is_valid_name(const char *name);

bool node_is_enabled(const vnode *n);

json_t * node_to_json(const vnode *n);

const char * node_to_json_str(vnode *n);

vsample * sample_alloc(unsigned len);

unsigned sample_length(vsample *smp);

void sample_decref(vsample *smp);

vsample * sample_pack(unsigned seq, struct timespec *ts_origin, struct timespec *ts_received, unsigned len, double *values);

void sample_unpack(vsample *s, unsigned *seq, struct timespec *ts_origin, struct timespec *ts_received, int *flags, unsigned *len, double *values);

int memory_init(int hugepages);
