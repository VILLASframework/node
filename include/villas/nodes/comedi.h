/** Node type: comedi
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

/**
 * @addtogroup comedi Comedi node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/list.h>

struct comedi {

};

/** @see node_type::print */
char * comedi_print(struct node *n);

/** @see node_type::parse */
int comedi_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int comedi_start(struct node *n);

/** @see node_type::close */
int comedi_stop(struct node *n);

/** @see node_type::read */
int comedi_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int comedi_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
