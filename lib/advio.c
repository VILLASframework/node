/** libcurl based remote IO
 *
 * Implements an fopen() abstraction allowing reading from URLs
 *
 * This file introduces a c library buffered I/O interface to
 * URL reads it supports fopen(), fread(), fgets(), feof(), fclose(),
 * rewind(). Supported functions have identical prototypes to their normal c
 * lib namesakes and are preceaded by a .
 *
 * Using this code you can replace your program's fopen() with afopen()
 * and fread() with afread() and it become possible to read remote streams
 * instead of (only) local files. Local files (ie those that can be directly
 * fopened) will drop back to using the underlying clib implementations
 *
 * This example requires libcurl 7.9.7 or later.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <curl/curl.h>

#include "utils.h"
#include "config.h"
#include "advio.h"

#define BAR_WIDTH 60 /**< How wide you want the progress meter to be. */

static int xferinfo(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	struct advio *af = (struct advio *) p;
	double curtime = 0;

	curl_easy_getinfo(af->curl, CURLINFO_TOTAL_TIME, &curtime);

	// ensure that the file to be downloaded is not empty
	// because that would cause a division by zero error later on
	if (dltotal <= 0.0)
		return 0;

	double frac = dlnow / dltotal;

	// part of the progressmeter that's already "full"
	int dotz = round(frac * BAR_WIDTH);

	// create the "meter"
	
	printf("%3.0f%% in %f s (%" CURL_FORMAT_CURL_OFF_T " / %" CURL_FORMAT_CURL_OFF_T ") [", frac * 100, curtime, dlnow, dltotal);

	// part  that's full already
	int i = 0;
	for ( ; i < dotz; i++)
		printf("=");

	// remaining part (spaces)
	for ( ; i < BAR_WIDTH; i++)
		printf(" ");

	// and back to line begin - do not forget the fflush to avoid output buffering problems!
	printf("]\r");
	fflush(stdout);

	return 0;
}

AFILE * afopen(const char *uri, const char *mode, int flags)
{
	CURLcode res;

	AFILE *af = alloc(sizeof(AFILE));
	
	if (flags & ADVIO_MEM)
		af->file = open_memstream(&af->buf, &af->size);
	else
		af->file = tmpfile();

	if (!af->file)
		goto out2;
	
	af->curl = curl_easy_init();
	if (!af->curl)
		goto out1;
	
	curl_easy_setopt(af->curl, CURLOPT_DEFAULT_PROTOCOL, "file");
	curl_easy_setopt(af->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(af->curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(af->curl, CURLOPT_URL, uri);
	curl_easy_setopt(af->curl, CURLOPT_WRITEDATA, af->file);
	curl_easy_setopt(af->curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
	curl_easy_setopt(af->curl, CURLOPT_XFERINFODATA, af);
	curl_easy_setopt(af->curl, CURLOPT_NOPROGRESS, 0L);
	
	res = curl_easy_perform(af->curl);
	switch (res) {
		case CURLE_OK:
			if (mode[0] == 'a')
				fseek(af->file, 0, SEEK_END);
			else if (mode[0] == 'r' || mode[0] == 'w')
				fseek(af->file, 0, SEEK_SET);
			break;

		/* The following error codes indicate that the file does not exist
		 * Check the fopen mode to see if we should continue with an emoty file */ 
		case CURLE_FILE_COULDNT_READ_FILE:
		case CURLE_TFTP_NOTFOUND:
		case CURLE_REMOTE_FILE_NOT_FOUND:
			if (mode[0] == 'a' || (mode[0] == 'w' && mode[1] == '+'))
				break;	/* its okay */

		/* If libcurl does not know the protocol, we will try it with the stdio */
		case CURLE_UNSUPPORTED_PROTOCOL:
			af->file = fopen(uri, mode);
			if (!af->file)
				goto out2;
		
		default:
		printf("avdio: %s\n", curl_easy_strerror(res));
			goto out0;	/* no please fail here */
	}
	
	af->uri = strdup(uri);
	af->flags = flags & ~ADVIO_DIRTY;
	
	return af;

out0:	curl_easy_cleanup(af->curl);
out1:	fclose(af->file);
out2:	free(af);

	return NULL;
}

int afclose(AFILE *af)
{
	int ret;
	
	ret = afflush(af);
	curl_easy_cleanup(af->curl);
	fclose(af->file);
	free(af);
	
	return ret;
}

int afflush(AFILE *af)
{
	int ret;
	
	/* Only upload file if it was changed */
	if (af->flags & ADVIO_DIRTY) {
		CURLcode res;
		long pos;
		
		/* Flushing a memory backed stream is sensless */
		if (!(af->flags & ADVIO_MEM)) {
			ret = fflush(af->file);
			if (ret)
				return ret;
		}

		curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(af->curl, CURLOPT_READDATA, af->file);

		pos = ftell(af->file); /* Remember old stream pointer */
		fseek(af->file, 0, SEEK_SET);

		res = curl_easy_perform(af->curl);
		
		fseek(af->file, pos, SEEK_SET); /* Restore old stream pointer */
		
		if (res != CURLE_OK)
			return -1;
		
		af->flags &= ADVIO_DIRTY;
	}

	return 0;
}

size_t afread(void *restrict ptr, size_t size, size_t nitems, AFILE *restrict af)
{
	return fread(ptr, size, nitems, af->file);
}

size_t afwrite(const void *restrict ptr, size_t size, size_t nitems, AFILE *restrict af)
{
	size_t ret;
	
	ret = fwrite(ptr, size, nitems, af->file);
	
	if (ret > 0)
		af->flags |= ADVIO_DIRTY;
	
	return ret;
}
