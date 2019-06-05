/** Node-type for exec node-types.
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
 * @ingroup node
 * @addtogroup exec Execute node-type as a sub-process
 * @{
 */

#pragma once

#include <villas/popen.hpp>
#include <villas/io.h>

/* Forward declarations */
struct node;
struct sample;
struct format_type;

/** Node-type for signal generation.
 * @see node_type
 */
struct exec {
	std::unique_ptr<villas::utils::Popen> proc;

	bool flush;
	std::string command;
	struct format_type *format;
	struct io io;
};

/** @see node_type::print */
char * exec_print(struct node *n);

/** @see node_type::parse */
int exec_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int exec_open(struct node *n);

/** @see node_type::close */
int exec_close(struct node *n);

/** @see node_type::read */
int exec_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int exec_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */
