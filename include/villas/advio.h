/** Advanced IO
 *
 */

#ifndef _ADVIO_H_
#define _ADVIO_H_

#include <stdio.h>

#include <curl/curl.h>

struct advio {
	CURL *curl;
	FILE *file;
	
	const char *url;
	
	bool dirty;		/**< The file contents have been modified. We need to upload */
};

typedef struct advio AFILE;

AFILE *afopen(const char *url, const char *mode);

int afclose(AFILE *file);

int afflush(AFILE *file);

/* The remaining functions from stdio are just replaced macros */

#define afeof(af)		feof(af->file)

size_t afread(void *restrict ptr, size_t size, size_t nitems, AFILE *restrict stream);
size_t afwrite(const void *restrict ptr, size_t size, size_t nitems, AFILE *restrict stream);

#endif /* _ADVIO_H_ */