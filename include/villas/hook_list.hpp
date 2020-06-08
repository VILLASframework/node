/** Hook list functions
 *
 * This file includes some examples.
 *
 * @file
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
 */

/**
 * @addtogroup hooks User-defined hook functions
 * @ingroup path
 * @{
 *********************************************************************************/

#pragma once

#include <jansson.h>

/* Forward declarations */
struct vlist;
struct sample;
struct vpath;
struct node;

int hook_list_init(struct vlist *hs);

int hook_list_destroy(struct vlist *hs);

/** Parses an object of hooks
 *
 * Example:
 *
 * {
 *    stats = {
 *       output = "stdout"
 *    },
 *    skip_first = {
 *       seconds = 10
 *    },
 *    hooks = [ "print" ]
 * }
 */
void hook_list_parse(struct vlist *hs, json_t *cfg, int mask, struct vpath *p, struct node *n);

void hook_list_prepare(struct vlist *hs, struct vlist *sigs, int mask, struct vpath *p, struct node *n);

int hook_list_prepare_signals(struct vlist *hs, struct vlist *signals);

int hook_list_add(struct vlist *hs, int mask, struct vpath *p, struct node *n);

int hook_list_process(struct vlist *hs, struct sample *smps[], unsigned cnt);

void hook_list_periodic(struct vlist *hs);

void hook_list_start(struct vlist *hs);

void hook_list_stop(struct vlist *hs);

struct vlist * hook_list_get_signals(struct vlist *hs);
