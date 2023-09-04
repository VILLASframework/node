/* Node C-API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include <jansson.h>

typedef void *vnode;
typedef void *vsample;

vnode * node_new(const char *id_str, const char *json_str);

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
