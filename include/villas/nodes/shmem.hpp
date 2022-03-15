/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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

#include <villas/node/memory.hpp>
#include <villas/pool.hpp>
#include <villas/queue.h>
#include <villas/node/config.hpp>
#include <villas/shmem.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct shmem {
	const char* out_name;   	/**< Name of the shm object for the output queue. */
	const char* in_name;    	/**< Name of the shm object for the input queue. */
	struct ShmemConfig conf; 	/**< Interface configuration struct. */
	char **exec;            	/**< External program to execute on start. */
	struct ShmemInterface intf;  	/**< Shmem interface */
};

char * shmem_print(NodeCompat *n);

int shmem_parse(NodeCompat *n, json_t *json);

int shmem_start(NodeCompat *n);

int shmem_stop(NodeCompat *n);

int shmem_init(NodeCompat *n);

int shmem_prepare(NodeCompat *n);

int shmem_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int shmem_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
