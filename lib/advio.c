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
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <curl/curl.h>

#include "utils.h"
#include "config.h"
#include "advio.h"

AFILE *afopen(const char *uri, const char *mode, int flags)
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
	
	curl_easy_setopt(af->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(af->curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(af->curl, CURLOPT_URL, uri);
	curl_easy_setopt(af->curl, CURLOPT_WRITEDATA, af->file);
	
	res = curl_easy_perform(af->curl);
	switch (res) {
		case CURLE_OK:
			switch (mode[0]) {
				case 'w':
				case 'r':
					fseek(af->file, 0, SEEK_SET);
					break;
				
				case 'a':
					fseek(af->file, 0, SEEK_END);
					break;
			}
			break;

		/* The following error codes indicate that the file does not exist
		 * Check the fopen mode to see if we should continue with an emoty file */ 
		case CURLE_FILE_COULDNT_READ_FILE:
		case CURLE_TFTP_NOTFOUND:
		case CURLE_REMOTE_FILE_NOT_FOUND:
			if (mode[0] == 'a' || (mode[0] == 'w' && mode[1] == '+'))
				break;	/* its okay */

		default:
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
