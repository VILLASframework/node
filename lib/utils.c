/** General purpose helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <ctype.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"

void print_copyright()
{
	printf("VILLASnode %s (built on %s %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	printf(" copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC\n");
	printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");
}

int version_parse(const char *s, struct version *v)
{
	return sscanf(s, "%u.%u", &v->major, &v->minor) != 2;
}

int version_compare(struct version *a, struct version *b) {
	int major = a->major - b->major;
	int minor = a->minor - b->minor;

	return major ? major : minor;
}

int strftimespec(char *dst, size_t max, const char *fmt, struct timespec *ts)
{
	int s, d;
	char fmt2[64], dst2[64];

	for (s=0, d=0; fmt[s] && d < 64;) {
		char c = fmt2[d++] = fmt[s++];
		if (c == '%') {
			char n = fmt[s];
			if (n == 'u') {
				fmt2[d++] = '0';
				fmt2[d++] = '3';
			}
			else
				fmt2[d++] = '%';
		}
	}
	
	/* Print sub-second part to format string */
	snprintf(dst2, sizeof(dst2), fmt2, ts->tv_nsec / 1000000);

	/* Print timestamp */
	struct tm tm;
	if (localtime_r(&(ts->tv_sec), &tm) == NULL)
		return -1;

	return strftime(dst, max, dst2, &tm);
}

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

void die()
{
	int zero = 0;
	log_outdent(&zero);
	abort();
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

cpu_set_t integer_to_cpuset(uintmax_t set)
{
	cpu_set_t cset;

	CPU_ZERO(&cset);

	for (int i = 0; i < sizeof(set) * 8; i++) {
		if (set & (1L << i))
			CPU_SET(i, &cset);
	}

	return cset;
}

#ifdef WITH_JANSSON
json_t * config_to_json(config_setting_t *cfg)
{
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_INT:	 return json_integer(config_setting_get_int(cfg));
		case CONFIG_TYPE_INT64:	 return json_integer(config_setting_get_int64(cfg));
		case CONFIG_TYPE_FLOAT:  return json_real(config_setting_get_float(cfg));
		case CONFIG_TYPE_STRING: return json_string(config_setting_get_string(cfg));
		case CONFIG_TYPE_BOOL:	 return json_boolean(config_setting_get_bool(cfg));

		case CONFIG_TYPE_ARRAY:
		case CONFIG_TYPE_LIST: {
			json_t *json = json_array();
			
			for (int i = 0; i < config_setting_length(cfg); i++)
				json_array_append_new(json, config_to_json(config_setting_get_elem(cfg, i)));
			
			return json;
		}
		
		case CONFIG_TYPE_GROUP: {
			json_t *json = json_object();
			
			for (int i = 0; i < config_setting_length(cfg); i++)
				json_object_set_new(json,
					config_setting_name(config_setting_get_elem(cfg, i)),
					config_to_json(config_setting_get_elem(cfg, i))
				);
			
			return json;
		}
		
		default:
			return json_object();
	}
}
#endif

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

int read_random(char *buf, size_t len)
{
	int ret, fd;
	
	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;
	
	ret = read(fd, buf, len);
	if (ret != len)
		ret = -1;
	else
		ret = 0;

	close(fd);

	return ret;
}

void printb(void *mem, size_t len)
{
	uint8_t *mem8 = (uint8_t *) mem;

	for (int i = 0; i < len; i++) {
		printf("%02hx ", mem8[i]);

		if (i % 16 == 15)
			printf("\n");
	}
}

void printdw(void *mem, size_t len)
{
	int columns = 4;

	uint32_t *mem32 = (uint32_t *) mem;

	for (int i = 0; i < len; i++) {
		if (i % columns == 0)
			printf("%#x: ", i * 4);

		printf("%08x ", mem32[i]);
		
		char *memc = (char *) &mem32[i];
		printf("%c%c%c%c ",
			isprint(memc[0]) ? memc[0] : ' ',
			isprint(memc[1]) ? memc[1] : ' ',
			isprint(memc[2]) ? memc[2] : ' ',
			isprint(memc[3]) ? memc[3] : ' '
		);

		if ((i+1) % columns == 0)
			printf("\n");
	}
}

void rdtsc_sleep(uint64_t nanosecs, uint64_t start)
{
	uint64_t cycles;

	cycles = (double) nanosecs / (1e9 / 3392389000);
	
	if (start == 0)
		start = rdtscp();
	
	do {
		__asm__("nop");
	} while (rdtscp() - start < cycles);
}