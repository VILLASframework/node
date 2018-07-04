/** libcurl based advanced IO aka ADVIO.
 *
 * This example requires libcurl 7.9.7 or later.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#include <curl/curl.h>

#include <villas/utils.h>
#include <villas/config.h>
#include <villas/advio.h>
#include <villas/crypt.h>

#define BAR_WIDTH 60 /**< How wide you want the progress meter to be. */

static int advio_trace(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
	const char *text;

	switch (type) {
		case CURLINFO_TEXT:
			text = "info";
			break;
		case CURLINFO_HEADER_OUT:
			text = "send header";
			break;
		case CURLINFO_DATA_OUT:
			text = "send data";
			break;
		case CURLINFO_HEADER_IN:
			text = "recv header";
			break;
		case CURLINFO_DATA_IN:
			text = "recv data";
			break;
		case CURLINFO_SSL_DATA_IN:
			text = "recv SSL data";
			break;
		case CURLINFO_SSL_DATA_OUT:
			text = "send SSL data";
			break;
		default: /* in case a new one is introduced to shock us */
			return 0;
	}

	debug(LOG_ADVIO | 5, "CURL: %s: %.*s", text, (int) size-1, data);

	return 0;
}

static char * advio_human_time(double t, char *buf, size_t len)
{
	int i = 0;
	const char *units[] = { "secs", "mins", "hrs", "days", "weeks", "months", "years" };
	int divs[]          = {     60,     60,    24,      7,       4,       12 };

	while (t > divs[i] && i < ARRAY_LEN(divs)) {
		t /= divs[i];
		i++;
	}

	snprintf(buf, len, "%.2f %s", t, units[i]);

	return buf;
}

static char * advio_human_size(double s, char *buf, size_t len)
{
	int i = 0;
	const char *units[] = { "B", "kiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };

	while (s > 1024 && i < ARRAY_LEN(units)) {
		s /= 1024;
		i++;
	}

	snprintf(buf, len, "%.*f %s", i ? 2 : 0, s, units[i]);

	return buf;
}

#if LIBCURL_VERSION_NUM >= 0x072000
static int advio_xferinfo(void *p, curl_off_t dl_total_bytes, curl_off_t dl_bytes, curl_off_t ul_total_bytes, curl_off_t ul_bytes)
{
	struct advio *af = (struct advio *) p;
	double cur_time, eta_time, estimated_time, frac;

	curl_easy_getinfo(af->curl, CURLINFO_TOTAL_TIME, &cur_time);

	/* Is this transaction an upload? */
	int upload = ul_total_bytes > 0;

	curl_off_t total_bytes = upload ? ul_total_bytes : dl_total_bytes;
	curl_off_t bytes       = upload ? ul_bytes       : dl_bytes;

	/* Are we finished? */
	if (bytes == 0)
		af->completed = 0;

	if (af->completed)
		return 0;

	/* Ensure that the file to be downloaded is not empty
	 * because that would cause a division by zero error later on */
	if (total_bytes <= 0)
		return 0;

	frac = (double) bytes / total_bytes;

	estimated_time = cur_time * (1.0 / frac);
	eta_time = estimated_time - cur_time;

	/* Print file sizes in human readable format */
	char buf[4][32];

	char *bytes_human = advio_human_size(bytes, buf[0], sizeof(buf[0]));
	char *total_bytes_human = advio_human_size(total_bytes, buf[1], sizeof(buf[1]));
	char *eta_time_human = advio_human_time(eta_time, buf[2], sizeof(buf[2]));

	/* Part of the progressmeter that's already "full" */
	int dotz = round(frac * BAR_WIDTH);

	/* Progress bar */
	fprintf(stderr, "\r[");

	for (int i = 0 ; i < BAR_WIDTH; i++) {
		if (upload)
			fputc(BAR_WIDTH - i > dotz ? ' ' : '<', stderr);
		else
			fputc(i > dotz ? ' ' : '>', stderr);
	}

	fprintf(stderr, "] ");

	/* Details */
	fprintf(stderr, "eta %-12s %12s of %-12s", eta_time_human, bytes_human, total_bytes_human);
	fflush(stderr);

	if (bytes == total_bytes) {
		af->completed = 1;
		fprintf(stderr, "\33[2K\r");
	}

	return 0;
}
#endif /* LIBCURL_VERSION_NUM >= 0x072000 */

int aislocal(const char *uri)
{
	char *sep;
	const char *supported_schemas[] = { "file", "http", "https", "tftp", "ftp", "scp", "sftp", "smb", "smbs" };

	sep = strstr(uri, "://");
	if (!sep)
		return 1; /* no schema, we assume its a local file */

	for (int i = 0; i < ARRAY_LEN(supported_schemas); i++) {
		if (!strncmp(supported_schemas[i], uri, sep - uri))
			return 0;
	}

	return -1; /* none of the supported schemas match. this is an invalid uri */
}

AFILE * afopen(const char *uri, const char *mode)
{
	int ret;
	char *sep, *cwd;

	AFILE *af = alloc(sizeof(AFILE));

	snprintf(af->mode, sizeof(af->mode), "%s", mode);

	sep = strstr(uri, "://");
	if (sep) {
		af->uri = strdup(uri);
		if (!af->uri)
			goto out2;
	}
	else { /* Open local file by prepending file:// schema. */
		if (strlen(uri) <= 1)
			return NULL;

		/* Handle relative paths */
		if (uri[0] != '/') {
			cwd = getcwd(NULL, 0);

			af->uri = strf("file://%s/%s", cwd, uri);

			free(cwd);
		}
		else
			af->uri = strf("file://%s", uri);
	}

	af->file = tmpfile();
	if (!af->file)
		goto out2;

	af->curl = curl_easy_init();
	if (!af->curl)
		goto out1;

	/* Setup libcurl handle */
	curl_easy_setopt(af->curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(af->curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(af->curl, CURLOPT_URL, af->uri);
	curl_easy_setopt(af->curl, CURLOPT_WRITEDATA, af->file);
	curl_easy_setopt(af->curl, CURLOPT_READDATA, af->file);

	curl_easy_setopt(af->curl, CURLOPT_DEBUGFUNCTION, advio_trace);
	curl_easy_setopt(af->curl, CURLOPT_VERBOSE, 1);

/* CURLOPT_XFERINFOFUNCTION is only supported on libcurl >= 7.32.0 */
#if LIBCURL_VERSION_NUM >= 0x072000
		curl_easy_setopt(af->curl, CURLOPT_XFERINFOFUNCTION, advio_xferinfo);
		curl_easy_setopt(af->curl, CURLOPT_XFERINFODATA, af);
#endif

	ret = adownload(af, 0);
	if (ret)
		goto out0;

	af->uploaded = 0;
	af->downloaded = 0;

	return af;

out0:	curl_easy_cleanup(af->curl);
out1:	fclose(af->file);
out2:	free(af->uri);
	free(af);

	return NULL;
}

int afclose(AFILE *af)
{
	int ret;

	ret = afflush(af);
	if (ret)
		return ret;

	curl_easy_cleanup(af->curl);

	ret = fclose(af->file);
	if (ret)
		return ret;

	free(af->uri);
	free(af);

	return 0;
}

int afseek(AFILE *af, long offset, int origin)
{
	long new, cur = aftell(af);

	switch (origin) {
		case SEEK_SET:
			new = offset;
			break;

		case SEEK_END:
			fseek(af->file, 0, SEEK_END);
			new = aftell(af);
			fseek(af->file, cur, SEEK_SET);
			break;

		case SEEK_CUR:
			new = cur + offset;
			break;

		default:
			return -1;
	}

	if (new < af->uploaded)
		af->uploaded = new;

	return fseek(af->file, offset, origin);
}

void arewind(AFILE *af)
{
	af->uploaded = 0;

	return rewind(af->file);
}

int afflush(AFILE *af)
{
	bool dirty;
	unsigned char hash[SHA_DIGEST_LENGTH];

	/* Check if fle was modified on disk by comparing hashes */
	sha1sum(af->file, hash);
	dirty = memcmp(hash, af->hash, sizeof(hash));

	if (dirty)
		return aupload(af, 1);

	return 0;
}

int aupload(AFILE *af, int resume)
{
	CURLcode res;

	long pos, end;

	double total_bytes = 0, total_time = 0;
	char buf[2][32];

	pos = aftell(af);
	fseek(af->file, 0, SEEK_END);
	end = aftell(af);
	fseek(af->file, 0, SEEK_SET);

	if (resume) {
		if (end == af->uploaded)
			return 0;

		char *size_human = advio_human_size(end - af->uploaded, buf[0], sizeof(buf[0]));

		info("Resume upload of %s of %s from offset %lu", af->uri, size_human, af->uploaded);
		curl_easy_setopt(af->curl, CURLOPT_RESUME_FROM, af->uploaded);
	}
	else {
		char *size_human = advio_human_size(end, buf[0], sizeof(buf[0]));

		info("Start upload of %s of %s", af->uri, size_human);
		curl_easy_setopt(af->curl, CURLOPT_RESUME_FROM, 0);
	}

	curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(af->curl, CURLOPT_INFILESIZE, end - af->uploaded);
	curl_easy_setopt(af->curl, CURLOPT_NOPROGRESS, !isatty(fileno(stderr)));

	res = curl_easy_perform(af->curl);

	fseek(af->file, pos, SEEK_SET); /* Restore old stream pointer */

	if (res != CURLE_OK)
		return -1;

	sha1sum(af->file, af->hash);

	curl_easy_getinfo(af->curl, CURLINFO_SIZE_UPLOAD, &total_bytes);
	curl_easy_getinfo(af->curl, CURLINFO_TOTAL_TIME, &total_time);

	char *total_bytes_human = advio_human_size(total_bytes, buf[0], sizeof(buf[0]));
	char *total_time_human  = advio_human_time(total_time,  buf[1], sizeof(buf[1]));

	info("Finished upload of %s in %s", total_bytes_human, total_time_human);

	af->uploaded += total_bytes;

	return 0;
}

int adownload(AFILE *af, int resume)
{
	CURLcode res;
	long code, pos;
	int ret;

	double total_bytes = 0, total_time = 0;
	char buf[2][32];

	pos = aftell(af);

	if (resume) {
		info("Resume download of %s from offset %lu", af->uri, af->downloaded);

		curl_easy_setopt(af->curl, CURLOPT_RESUME_FROM, af->downloaded);
	}
	else {
		info("Start download of %s", af->uri);

		rewind(af->file);
		curl_easy_setopt(af->curl, CURLOPT_RESUME_FROM, 0);
	}

	curl_easy_setopt(af->curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(af->curl, CURLOPT_NOPROGRESS, !isatty(fileno(stderr)));

	res = curl_easy_perform(af->curl);

	switch (res) {
		case CURLE_OK:
			curl_easy_getinfo(af->curl, CURLINFO_SIZE_DOWNLOAD, &total_bytes);
			curl_easy_getinfo(af->curl, CURLINFO_TOTAL_TIME, &total_time);

			char *total_bytes_human = advio_human_size(total_bytes, buf[0], sizeof(buf[0]));
			char *total_time_human  = advio_human_time(total_time,  buf[1], sizeof(buf[1]));

			info("Finished download of %s in %s", total_bytes_human, total_time_human);

			af->downloaded += total_bytes;
			af->uploaded = af->downloaded;

			res = curl_easy_getinfo(af->curl, CURLINFO_RESPONSE_CODE, &code);
			if (res)
				return -1;

			switch (code) {
				case   0:
				case 200: goto exist;
				case 404: goto notexist;
				default:  return -1;
			}

		/* The following error codes indicate that the file does not exist
		 * Check the fopen mode to see if we should continue with an emoty file */
		case CURLE_FILE_COULDNT_READ_FILE:
		case CURLE_TFTP_NOTFOUND:
		case CURLE_REMOTE_FILE_NOT_FOUND:
			info("File does not exist.");
			goto notexist;

		/* If libcurl does not know the protocol, we will try it with the stdio */
		case CURLE_UNSUPPORTED_PROTOCOL:
			af->file = fopen(af->uri, af->mode);
			if (!af->file)
				return -1;

		default:
			error("Failed to download file: %s: %s", af->uri, curl_easy_strerror(res));
			return -1;
	}


notexist: /* File does not exist */

	/* According to mode the file must exist! */
	if (af->mode[1] != '+' || (af->mode[0] != 'w' && af->mode[0] != 'a')) {
		errno = ENOENT;
		return -1;
	}

	/* If we receive a 404, we discard the already received error page
	 * and start with an empty file. */
	fflush(af->file);
	ret = ftruncate(fileno(af->file), 0);
	if (ret)
		return ret;

exist: /* File exists */
	if (resume)
		afseek(af, pos, SEEK_SET);
	else if (af->mode[0] == 'a')
		afseek(af, 0, SEEK_END);
	else if (af->mode[0] == 'r' || af->mode[0] == 'w')
		afseek(af, 0, SEEK_SET);

	sha1sum(af->file, af->hash);

	return 0;
}
