/** Unit tests for advio
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <villas/utils.h>
#include <villas/advio.h>

#include <errno.h>

/** This URI points to a Sciebo share which contains some test files.
 * The Sciebo share is read/write accessible via WebDAV. */
#define BASE_URI "https://1Nrd46fZX8HbggT:badpass@rwth-aachen.sciebo.de/public.php/webdav/node/tests"

#define TEST_DATA_DOWNLOAD	"ook4iekohC2Teegoghu6ayoo1OThooregheebaet8Zod1angah0che7quai4ID7A"
#define TEST_DATA_UPLOAD	"jaeke4quei3oongeebuchahz9aabahkie8oor3Gaejem1AhSeph5Ahch9ohz3eeh"
#define TEST_DATA_APPEND1	"xa5gieTohlei9iu1uVaePae6Iboh3eeheeme5iejue5sheshae4uzisha9Faesei"
#define TEST_DATA_APPEND2	"bitheeRae7igee2miepahJaefoGad1Ooxeif0Mooch4eojoumueYahn4ohc9poo2"

Test(advio, download)
{
	AFILE *af;
	int ret;
	size_t len;
	char buffer[64];

	af = afopen(BASE_URI "/download" , "r");
	cr_assert(af, "Failed to download file");

	len = afread(buffer, 1, sizeof(buffer), af);
	
	cr_assert_gt(len, 0);
	cr_assert_arr_eq(buffer, TEST_DATA_DOWNLOAD, strlen(TEST_DATA_DOWNLOAD));
	
	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");
}

Test(advio, upload)
{
	AFILE *af;
	int ret;
	
	size_t len;
	size_t tlen = strlen(TEST_DATA_UPLOAD);
	
	char buffer[tlen];
	
	/* Open file for writing */
	af = afopen(BASE_URI "/upload", "w+");
	cr_assert(af, "Failed to download file");
	
	len = afwrite(TEST_DATA_UPLOAD, 1, strlen(TEST_DATA_UPLOAD), af);
	cr_assert_eq(len, strlen(TEST_DATA_UPLOAD));
	
	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");
	
	/* Open for reading and comparison */
	af = afopen(BASE_URI "/upload", "r");
	cr_assert(af, "Failed to download file");
	
	len = afread(buffer, 1, strlen(TEST_DATA_UPLOAD), af);
	cr_assert_eq(len, strlen(TEST_DATA_UPLOAD));
	
	cr_assert_arr_eq(buffer, TEST_DATA_UPLOAD, len);
	
	ret = afclose(af);
	cr_assert(ret == 0, "Failed to close file");
}

Test(advio, append)
{
	AFILE *af;
	
	int ret;

	size_t len;
	size_t tlen = strlen(TEST_DATA_APPEND1) + strlen(TEST_DATA_APPEND2);

	char buffer[tlen];

	/* Open file for writing first chunk */
	af = afopen(BASE_URI "/append", "w+");
	cr_assert(af, "Failed to download file");

	len = afwrite(TEST_DATA_APPEND1, 1, strlen(TEST_DATA_APPEND1), af);
	cr_assert_eq(len, strlen(TEST_DATA_APPEND1));

	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");

	/* Open file for writing second chunk */
	af = afopen(BASE_URI "/append", "a");
	cr_assert(af, "Failed to download file");
	
	len = afwrite(TEST_DATA_APPEND1, 1, strlen(TEST_DATA_APPEND2), af);
	cr_assert_eq(len, strlen(TEST_DATA_APPEND2));
	
	ret = afclose(af);
	cr_assert_eq(ret, 0, "Failed to close/upload file");
	
	/* Open for reading and comparison */
	af = afopen(BASE_URI "/append", "r");
	cr_assert(af, "Failed to download file");
	
	len = afread(buffer, 1, tlen, af);
	cr_assert_eq(len, tlen);
	
	ret = afclose(af);
	cr_assert(ret == 0, "Failed to close file");

	cr_assert_arr_eq(buffer,                             TEST_DATA_APPEND1, strlen(TEST_DATA_APPEND1));
	cr_assert_arr_eq(buffer + strlen(TEST_DATA_APPEND1), TEST_DATA_APPEND2, strlen(TEST_DATA_APPEND2));
}