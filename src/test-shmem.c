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

#include <string.h>

#include <villas/config.h>
#include <villas/log.h>
#include <villas/node.h>
#include <villas/pool.h>
#include <villas/sample.h>
#include <villas/shmem.h>
#include <villas/utils.h>

void *base;
struct shmem_int shm;

void usage()
{
	printf("Usage: villas-test-shmem WNAME VECTORIZE\n");
	printf("  WNAME     name of the shared memory object for the output queue\n");
	printf("  RNAME     name of the shared memory object for the input queue\n");
	printf("  VECTORIZE maximum number of samples to read/write at a time\n");
}

void quit(int sig)
{
	shmem_int_close(&shm);
	exit(1);
}

int main(int argc, char* argv[])
{
	struct log log;
	int readcnt, writecnt, avail;
	struct shmem_conf conf = {
		.queuelen = DEFAULT_SHMEM_QUEUELEN,
		.samplelen = DEFAULT_SHMEM_SAMPLELEN,
		.polling = 0,
	};

	log_init(&log, V, LOG_ALL);
	log_start(&log);


	if (argc != 4) {
		usage();
		return 1;
	}

	char *wname = argv[1];
	char *rname = argv[2];
	int vectorize = atoi(argv[3]);

	if (shmem_int_open(wname, rname, &shm, &conf) < 0)
		serror("Failed to open shmem interface");

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	struct sample *insmps[vectorize], *outsmps[vectorize];

	while (1) {
		readcnt = shmem_int_read(&shm, insmps, vectorize);
		if (readcnt == -1) {
			printf("Node stopped, exiting");
			break;
		}

		avail = shmem_int_alloc(&shm, outsmps, readcnt);
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

		writecnt = shmem_int_write(&shm, outsmps, avail);
		if (writecnt < avail)
			warn("Short write");

		info("Read / Write: %d / %d", readcnt, writecnt);
	}
	shmem_int_close(&shm);
}
