/** Unit tests for advio
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

#include <unistd.h>

#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <villas/utils.h>
#include <villas/advio.h>

/** This URI points to a Sciebo share which contains some test files.
 * The Sciebo share is read/write accessible via WebDAV. */
#define BASE_URI "https://1Nrd46fZX8HbggT:badpass@rwth-aachen.sciebo.de/public.php/webdav/node/tests"

Test(advio, islocal)
{
	int ret;

	ret = aislocal("/var/log/villas/dta.dat");
	cr_assert_eq(ret, 1);

	ret = aislocal("http://www.google.de");
	cr_assert_eq(ret, 0);

	ret = aislocal("torrent://www.google.de");
	cr_assert_eq(ret, -1);
}

Test(advio, local)
{
	AFILE *af;
	int ret;
	char *buf = NULL;
	size_t buflen = 0;

	/* We open this file and check the first line */
	af = afopen(__FILE__, "r");
	cr_assert(af, "Failed to open local file");

	ret = getline(&buf, &buflen, af->file);
	cr_assert_gt(ret, 1);
	cr_assert_str_eq(buf, "/** Unit tests for advio\n");
}

Test(advio, download)
{
	AFILE *af;
	int ret;
	size_t len;
	char buffer[64];
	char expect[64] = "ook4iekohC2Teegoghu6ayoo1OThooregheebaet8Zod1angah0che7quai4ID7A";

	af = afopen(BASE_URI "/download" , "r");
	cr_assert(af, "Failed to download file");

	len = afread(buffer, 1, sizeof(buffer), af);
	cr_assert_gt(len, 0, "len=%zu, feof=%u", len, afeof(af));

	cr_assert_arr_eq(buffer, expect, sizeof(expect));

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close file");
}

Test(advio, download_large)
{
	AFILE *af;
	int ret;

	af = afopen(BASE_URI "/download-large" , "r");
	cr_assert(af, "Failed to download file");

	char line[4096];

	char *f;

	f = afgets(line, 4096, af);
	cr_assert_not_null(f);

	/* Check first line */
	cr_assert_str_eq(line, "# VILLASnode signal params: type=mixed, values=4, rate=1000.000000, limit=100000, amplitude=1.000000, freq=1.000000\n");

	while(afgets(line, 4096, af));

	/* Check last line */
	cr_assert_str_eq(line, "1497710478.862332239(99999)	0.752074	-0.006283	1.000000	0.996000\n");

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close file");
}

Test(advio, resume)
{
	int ret;
	char *retp;
	AFILE *af1, *af2;
	char *fn, dir[] = "/tmp/temp.XXXXXX";
	char line1[32];
	char *line2 = NULL;
	size_t linelen = 0;

	retp = mkdtemp(dir);
	cr_assert_not_null(retp);

	ret = asprintf(&fn, "%s/file", dir);
	cr_assert_gt(ret, 0);

	af1 = afopen(fn, "w+");
	cr_assert_not_null(af1);

	/* We flush once the empty file in order to upload an empty file. */
	aupload(af1, 0);

	af2 = afopen(fn, "r");
	cr_assert_not_null(af2);

	for (int i = 0; i < 100; i++) {
		snprintf(line1, sizeof(line1), "This is line %d\n", i);

		afputs(line1, af1);
		aupload(af1, 1);

		adownload(af2, 1);

		ret = agetline(&line2, &linelen, af2);
		cr_assert_gt(ret, 0);

		cr_assert_str_eq(line1, line2);
	}

	ret = afclose(af1);
	cr_assert_eq(ret, 0);

	ret = afclose(af2);
	cr_assert_eq(ret, 0);

	ret = unlink(fn);
	cr_assert_eq(ret, 0);

	ret = rmdir(dir);
	cr_assert_eq(ret, 0);

	free(line2);
}

Test(advio, upload)
{
	AFILE *af;
	int ret;
	size_t len;

	char upload[64];
	char buffer[64];

	/* Get some random data to upload */
	len = read_random(upload, sizeof(upload));
	cr_assert_eq(len, sizeof(upload));

	/* Open file for writing */
	af = afopen(BASE_URI "/upload", "w+");
	cr_assert(af, "Failed to download file");

	len = afwrite(upload, 1, sizeof(upload), af);
	cr_assert_eq(len, sizeof(upload));

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");

	/* Open for reading and comparison */
	af = afopen(BASE_URI "/upload", "r");
	cr_assert(af, "Failed to download file");

	len = afread(buffer, 1, sizeof(upload), af);
	cr_assert_eq(len, sizeof(upload));

	cr_assert_arr_eq(buffer, upload, len);

	ret = afclose(af);
	cr_assert(ret == 0, "Failed to close file");
}

Test(advio, append)
{
	AFILE *af;
	int ret;
	size_t len;

	char append1[64] = "xa5gieTohlei9iu1uVaePae6Iboh3eeheeme5iejue5sheshae4uzisha9Faesei";
	char append2[64] = "bitheeRae7igee2miepahJaefoGad1Ooxeif0Mooch4eojoumueYahn4ohc9poo2";
	char expect[128] = "xa5gieTohlei9iu1uVaePae6Iboh3eeheeme5iejue5sheshae4uzisha9FaeseibitheeRae7igee2miepahJaefoGad1Ooxeif0Mooch4eojoumueYahn4ohc9poo2";
	char buffer[128];

	/* Open file for writing first chunk */
	af = afopen(BASE_URI "/append", "w+");
	cr_assert(af, "Failed to download file");

	/* The append file might already exist and be not empty from a previous run. */
	ret = ftruncate(afileno(af), 0);
	cr_assert_eq(ret, 0);

	char c;
	fseek(af->file, 0, SEEK_SET);
	if (af->file) {
	    while ((c = getc(af->file)) != EOF)
	        putchar(c);
	}

	len = afwrite(append1, 1, sizeof(append1), af);
	cr_assert_eq(len, sizeof(append1));

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");

	/* Open file for writing second chunk */
	af = afopen(BASE_URI "/append", "a");
	cr_assert(af, "Failed to download file");

	len = afwrite(append2, 1, sizeof(append2), af);
	cr_assert_eq(len, sizeof(append2));

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");

	/* Open for reading and comparison */
	af = afopen(BASE_URI "/append", "r");
	cr_assert(af, "Failed to download file");

	len = afread(buffer, 1, sizeof(buffer), af);
	cr_assert_eq(len, sizeof(buffer));

	ret = afclose(af);
	cr_assert(ret == 0, "Failed to close file");

	cr_assert_arr_eq(buffer, expect, sizeof(expect));
}
