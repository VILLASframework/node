/** libcurl based remote IO
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *	 This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *	 Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "curlio.h"

/*****************************************************************************
 *
 * This example source code introduces a c library buffered I/O interface to
 * URL reads it supports fopen(), fread(), fgets(), feof(), fclose(),
 * rewind(). Supported functions have identical prototypes to their normal c
 * lib namesakes and are preceaded by url_ .
 *
 * Using this code you can replace your program's fopen() with url_fopen()
 * and fread() with url_fread() and it become possible to read remote streams
 * instead of (only) local files. Local files (ie those that can be directly
 * fopened) will drop back to using the underlying clib implementations
 *
 * See the main() function at the bottom that shows an app that retrives from a
 * specified url using fgets() and fread() and saves as two output files.
 *
 * Copyright (c) 2003 Simtec Electronics
 *
 * Re-implemented by Vincent Sanders <vince@kyllikki.org> with extensive
 * reference to original curl example code
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *		notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *		notice, this list of conditions and the following disclaimer in the
 *		documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *		derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This example requires libcurl 7.9.7 or later.
 */
/* <DESC>
 * implements an fopen() abstraction allowing reading from URLs
 * </DESC>
 */

#include <stdio.h>
#include <string.h>
#ifndef WIN32
  #include <sys/time.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include <curl/curl.h>

enum fcurl_type_e {
	CFTYPE_NONE = 0,
	CFTYPE_FILE = 1,
	CFTYPE_CURL = 2
};

struct fcurl_data
{
	enum fcurl_type_e type;	 /**< Type of handle */
	union {
		CURL *curl;
		FILE *file;
	} handle;		/**< Handle */

	char *buffer;		/**< Buffer to store cached data*/
	size_t buffer_len;	/**< Currently allocated buffers length */
	size_t buffer_pos;	/**< End of data in buffer*/
	int still_running;	/**< Is background url fetch still in progress */
};

/* We use a global one for convenience */
CURLM *multi_handle;

/* libcurl calls this routine to get more data */
static size_t write_callback(char *buffer, size_t size, size_t nitems, void *userp)
{
	char *newbuff;
	size_t rembuff;

	URL_FILE *url = (URL_FILE *)userp;
	size *= nitems;

	rembuff = url->buffer_len - url->buffer_pos; /* Remaining space in buffer */

	if (size > rembuff) {
		/* Not enough space in buffer */
		newbuff = realloc(url->buffer, url->buffer_len + (size - rembuff));
		if (newbuff == NULL) {
			fprintf(stderr, "callback buffer grow failed\n");
			size = rembuff;
		}
		else {
			/* Realloc succeeded increase buffer size*/
			url->buffer_len += size - rembuff;
			url->buffer = newbuff;
		}
	}

	memcpy(&url->buffer[url->buffer_pos], buffer, size);
	url->buffer_pos += size;

	return size;
}

/* Use to attempt to fill the read buffer up to requested number of bytes */
static int fill_buffer(URL_FILE *file, size_t want)
{
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	struct timeval timeout;
	int rc;
	CURLMcode mc; /* curl_multi_fdset() return code */

	/* Only attempt to fill buffer if transactions still running and buffer
	 * doesn't exceed required size already
	 */
	if ((!file->still_running) || (file->buffer_pos > want))
		return 0;

	/* Attempt to fill buffer */
	do {
		int maxfd = -1;
		long curl_timeo = -1;

		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		/* Set a suitable timeout to fail on */
		timeout.tv_sec = 60; /* 1 minute */
		timeout.tv_usec = 0;

		curl_multi_timeout(multi_handle, &curl_timeo);
		if (curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if (timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		/* Get file descriptors from the transfers */
		mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

		if (mc != CURLM_OK) {
			fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
			break;
		}

		/* On success the value of maxfd is guaranteed to be >= -1. We call
		 * select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
		 * no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
		 * to sleep 100ms, which is the minimum suggested value in the
		 * curl_multi_fdset() doc. */

		if (maxfd == -1) {
#ifdef _WIN32
			Sleep(100);
			rc = 0;
#else
			/* Portable sleep for platforms other than Windows. */
			struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
			rc = select(0, NULL, NULL, NULL, &wait);
#endif
		}
		else {
			/* Note that on some platforms 'timeout' may be modified by select().
				 If you need access to the original value save a copy beforehand. */
			rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		switch (rc) {
			case -1:
				/* Select error */
				break;

			case 0:
			default:
				/* Timeout or readable/writable sockets */
				curl_multi_perform(multi_handle, &file->still_running);
				break;
		}
	} while(file->still_running && (file->buffer_pos < want));
	return 1;
}

/* Use to remove want bytes from the front of a files buffer */
static int use_buffer(URL_FILE *file, size_t want)
{
	/* Sort out buffer */
	if ((file->buffer_pos - want) <= 0) {
		/* Ditch buffer - write will recreate */
		free(file->buffer);
		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;
	}
	else {
		/* Move rest down make it available for later */
		memmove(file->buffer, &file->buffer[want], (file->buffer_pos - want));

		file->buffer_pos -= want;
	}
	return 0;
}

URL_FILE *url_fopen(const char *url, const char *operation)
{
	/* This code could check for URLs or types in the 'url' and
	   basically use the real fopen() for standard files */

	URL_FILE *file;
	(void)operation;

	file = malloc(sizeof(URL_FILE));
	if (!file)
		return NULL;

	memset(file, 0, sizeof(URL_FILE));

	file->handle.file = fopen(url, operation);
	if (file->handle.file)
		file->type = CFTYPE_FILE; /* Marked as standard file */
	else {
		file->type = CFTYPE_CURL; /* Marked as URL */
		file->handle.curl = curl_easy_init();

		curl_easy_setopt(file->handle.curl, CURLOPT_URL, url);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION, write_callback);

		if (!multi_handle)
			multi_handle = curl_multi_init();

		curl_multi_add_handle(multi_handle, file->handle.curl);

		/* Lets start the fetch */
		curl_multi_perform(multi_handle, &file->still_running);

		if ((file->buffer_pos == 0) && (!file->still_running)) {
			/* If still_running is 0 now, we should return NULL */

			/* Make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(multi_handle, file->handle.curl);

			/* Cleanup */
			curl_easy_cleanup(file->handle.curl);

			free(file);

			file = NULL;
		}
	}

	return file;
}

int url_fclose(URL_FILE *file)
{
	int ret = 0; /* Default is good return */

	switch (file->type) {
		case CFTYPE_FILE:
			ret = fclose(file->handle.file); /* Passthrough */
			break;

		case CFTYPE_CURL:
			/* Make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(multi_handle, file->handle.curl);

			/* Cleanup */
			curl_easy_cleanup(file->handle.curl);
			break;

		default: /* Unknown or supported type - oh dear */
			ret = EOF;
			errno = EBADF;
			break;
	}

	free(file->buffer);/* Free any allocated buffer space */
	free(file);

	return ret;
}

int url_feof(URL_FILE *file)
{
	int ret = 0;

	switch (file->type) {
		case CFTYPE_FILE:
			ret = feof(file->handle.file);
			break;

		case CFTYPE_CURL:
			if ((file->buffer_pos == 0) && (!file->still_running))
				ret = 1;
			break;

		default: /* Unknown or supported type - oh dear */
			ret = -1;
			errno = EBADF;
			break;
	}

	return ret;
}

size_t url_fread(void *ptr, size_t size, size_t nmemb, URL_FILE *file)
{
	size_t want;

	switch (file->type) {
		case CFTYPE_FILE:
			want = fread(ptr, size, nmemb, file->handle.file);
			break;

		case CFTYPE_CURL:
			want = nmemb * size;

			fill_buffer(file, want);

			/* Check if theres data in the buffer - if not fill_buffer()
			 * either errored or EOF */
			if (!file->buffer_pos)
				return 0;

			/* Ensure only available data is considered */
			if (file->buffer_pos < want)
				want = file->buffer_pos;

			/* Xfer data to caller */
			memcpy(ptr, file->buffer, want);

			use_buffer(file, want);

			want = want / size; /* Number of items */
			break;

		default: /* Unknown or supported type - oh dear */
			want = 0;
			errno = EBADF;
			break;
	}

	return want;
}

char *url_fgets(char *ptr, size_t size, URL_FILE *file)
{
	size_t want = size - 1; /* Always need to leave room for zero termination */
	size_t loop;

	switch (file->type) {
		case CFTYPE_FILE:
			ptr = fgets(ptr, (int)size, file->handle.file);
			break;

		case CFTYPE_CURL:
			fill_buffer(file, want);

			/* Check if theres data in the buffer - if not fill either errored or
			 * EOF */
			if (!file->buffer_pos)
				return NULL;

			/* Ensure only available data is considered */
			if (file->buffer_pos < want)
				want = file->buffer_pos;

			/* Buffer contains data */
			/* Look for newline or eof */
			for (loop = 0; loop < want; loop++) {
				if (file->buffer[loop] == '\n') {
					want = loop+1; /* Include newline */
					break;
				}
			}

			/* Xfer data to caller */
			memcpy(ptr, file->buffer, want);
			ptr[want] = 0; /* Allways null terminate */

			use_buffer(file, want);

			break;

		default: /* Unknown or supported type - oh dear */
			ptr = NULL;
			errno = EBADF;
			break;
	}

	return ptr;/*success */
}

void url_rewind(URL_FILE *file)
{
	switch (file->type) {
		case CFTYPE_FILE:
			rewind(file->handle.file); /* Passthrough */
			break;

		case CFTYPE_CURL:
			/* Halt transaction */
			curl_multi_remove_handle(multi_handle, file->handle.curl);

			/* Restart */
			curl_multi_add_handle(multi_handle, file->handle.curl);

			/* Ditch buffer - write will recreate - resets stream pos */
			free(file->buffer);
			file->buffer = NULL;
			file->buffer_pos = 0;
			file->buffer_len = 0;

			break;

		default: /* Unknown or supported type - oh dear */
			break;
	}
}
