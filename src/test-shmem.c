/** Test "client" for the shared memory interface.
 *
 * Waits on the incoming queue, prints received samples and writes them
 * back to the other queue.
 *
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/log.h>
#include <villas/node.h>
#include <villas/nodes/shmem.h>
#include <villas/pool.h>
#include <villas/sample.h>
#include <villas/shmem.h>
#include <villas/utils.h>

#include <string.h>

void *base;
struct shmem_shared *shared;

void usage()
{
	printf("Usage: villas-test-shmem SHM_NAME VECTORIZE\n");
	printf("  SHMNAME   name of the shared memory object\n");
	printf("  VECTORIZE maximum number of samples to read/write at a time\n");
}

void quit(int sig)
{
	shmem_shared_close(shared, base);
	exit(1);
}

int main(int argc, char* argv[])
{
	struct log log;
	
	log_init(&log, V, LOG_ALL);
	log_start(&log);
	
	int readcnt, writecnt, avail;

	if (argc != 3) {
		usage();
		return 1;
	}
	
	char *object = argv[1];
	int vectorize = atoi(argv[2]);

	shared = shmem_shared_open(object, &base);
	if (!shared)
		serror("Failed to open shmem interface");

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	struct sample *insmps[vectorize], *outsmps[vectorize];
	
	while (1) {
		readcnt = shmem_shared_read(shared, insmps, vectorize);
		if (readcnt == -1) {
			printf("Node stopped, exiting");
			break;
		}

		avail = sample_alloc(&shared->pool, outsmps, readcnt);
		if (avail < readcnt)
			warn("Pool underrun: %d / %d\n", avail, readcnt);

		for (int i = 0; i < avail; i++) {
			outsmps[i]->sequence = insmps[i]->sequence;
			outsmps[i]->ts = insmps[i]->ts;

			int len = MIN(insmps[i]->length, outsmps[i]->capacity);
			memcpy(outsmps[i]->data, insmps[i]->data, SAMPLE_DATA_LEN(len));

			outsmps[i]->length = len;
		}

		for (int i = 0; i < readcnt; i++)
			sample_put(insmps[i]);

		writecnt = shmem_shared_write(shared, outsmps, avail);
		if (writecnt < avail)
			warn("Short write");
		
		info("Read / Write: %d / %d", readcnt, writecnt);
	}
}
