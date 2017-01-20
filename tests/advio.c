#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <villas/advio.h>

#include <errno.h>
#include <error.h>

Test(advio, download)
{
	AFILE *af;
	size_t len;
	int ret;
	char buffer[1024];
	
	const char *url = "http://www.textfiles.com/100/angela.art";

	af = afopen(url, "r");
	cr_assert(af, "Failed to download file");
	cr_log_info("Opened file %s", url);

	while (!afeof(af)) {
		errno = 0;
		len = afread(buffer, 1, sizeof(buffer), af);
		cr_log_info("Read %zu bytes", len);

		fwrite(buffer, 1, len, stdout);
	}
	
	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");
}

Test(advio, upload)
{
	AFILE *af;
	size_t len1, len2;
	int ret;
	
	const char *uri = "file:///tmp/test.txt";
	char rnd[128];
	char buffer[128];
	
	/* Get some random bytes */
	FILE *r = fopen("/dev/urandom", "r");
	cr_assert(r);
	
	len1 = fread(rnd, 1, sizeof(rnd), r);

	/* Open file for writing */
	af = afopen(uri, "w+");
	cr_assert(af, "Failed to download file");
	
	len2 = afwrite(rnd, 1, len1, af);
	cr_assert_eq(len1, len2);
	
	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");
	
	/* Open for reading and comparison */
	af = afopen(uri, "r");
	cr_assert(af, "Failed to download file");
	
	len2 = afread(buffer, 1, len1, af);
	cr_assert_arr_eq(rnd, buffer, len2);
	
	ret = afclose(af);
	cr_assert(ret == 0, "Failed to close file");
}