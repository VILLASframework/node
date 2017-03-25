/** libcurl based advanced IO aka ADVIO.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stdio.h>

#include <curl/curl.h>

#include "utils.h"

struct advio {
	CURL *curl;
	FILE *file;
	
	unsigned char hash[SHA_DIGEST_LENGTH];

	char mode[2];
	char *uri;
};

typedef struct advio AFILE;

/* The remaining functions from stdio are just replaced macros */
#define afeof(af)			feof((af)->file)
#define aftell(af)			ftell((af)->file)
#define arewind(af)			rewind((af)->file)
#define afileno(af)			fileno((af)->file)
#define afread(ptr, sz, nitems, af)	fread(ptr, sz, nitems, (af)->file)
#define afwrite(ptr, sz, nitems, af)	fwrite(ptr, sz, nitems, (af)->file)

/* Extensions */
#define auri(af)			((af)->uri)
#define ahash(af)			((af)->hash)

AFILE *afopen(const char *url, const char *mode);

int afclose(AFILE *file);
int afflush(AFILE *file);
int adownload(AFILE *af);
int aupload(AFILE *af);
