/** Node type: mqtt_unthreaded
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
 * @addtogroup mqtt_unthreaded mqtt unthreaded node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/nodes/mqtt.hpp>

/** @see node_type::open */
int mqtt_unthreaded_start(struct node *n);

/** @see node_type::close */
int mqtt_unthreaded_stop(struct node *n);

/** @see node_type::read */
int mqtt_unthreaded_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int mqtt_unthreaded_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** perform MQTT network operations */
int mqtt_unthreaded_loop(struct node *n);

/** @} */
