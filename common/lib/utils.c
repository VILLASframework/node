/** General purpose helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>

#include <villas/config.h>
#include <villas/utils.h>

pthread_t main_thread;

double box_muller(float m, float s)
{
	double x1, x2, y1;
	static double y2;
	static int use_last = 0;

	if (use_last) {		/* use value from previous call */
		y1 = y2;
		use_last = 0;
	}
	else {
		double w;
		do {
			x1 = 2.0 * randf() - 1.0;
			x2 = 2.0 * randf() - 1.0;
			w = x1*x1 + x2*x2;
		} while (w >= 1.0);

		w = sqrt(-2.0 * log(w) / w);
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}

	return m + y1 * s;
}

double randf()
{
	return (double) random() / RAND_MAX;
}

char * strcatf(char **dest, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vstrcatf(dest, fmt, ap);
	va_end(ap);

	return *dest;
}

char * vstrcatf(char **dest, const char *fmt, va_list ap)
{
	char *tmp;
	int n = *dest ? strlen(*dest) : 0;
	int i = vasprintf(&tmp, fmt, ap);

	*dest = (char *)(realloc(*dest, n + i + 1));
	if (*dest != NULL)
		strncpy(*dest+n, tmp, i + 1);

	free(tmp);

	return *dest;
}

void * alloc(size_t bytes)
{
	void *p = malloc(bytes);
	if (!p)
		error("Failed to allocate memory");

	memset(p, 0, bytes);

	return p;
}

void * memdup(const void *src, size_t bytes)
{
	void *dst = alloc(bytes);

	memcpy(dst, src, bytes);

	return dst;
}

void killme(int sig)
{
	/* Send only to main thread in case the ID was initilized by signals_init() */
	if (main_thread)
		pthread_kill(main_thread, sig);
	else
		kill(0, sig);
}

pid_t spawn(const char* name, char *const argv[])
{
	pid_t pid;

	pid = fork();
	switch (pid) {
		case -1: return -1;
		case 0:  return execvp(name, (char * const*) argv);
	}

	return pid;
}

size_t strlenp(const char *str)
{
	size_t sz = 0;

	for (const char *d = str; *d; d++) {
		const unsigned char *c = (const unsigned char *) d;

		if (isprint(*c))
			sz++;
		else if (c[0] == '\b')
			sz--;
		else if (c[0] == '\t')
			sz += 4; /* tab width == 4 */
		/* CSI sequence */
		else if (c[0] == '\e' && c[1] == '[') {
			c += 2;
			while (*c && *c != 'm')
				c++;
		}
		/* UTF-8 */
		else if (c[0] >= 0xc2 && c[0] <= 0xdf) {
			sz++;
			c += 1;
		}
		else if (c[0] >= 0xe0 && c[0] <= 0xef) {
			sz++;
			c += 2;
		}
		else if (c[0] >= 0xf0 && c[0] <= 0xf4) {
			sz++;
			c += 3;
		}

		d = (const char *) c;
	}

	return sz;
}
