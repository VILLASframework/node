#pragma once

#include <stdio.h>

/** Calculate SHA1 hash of complete file \p f and place it into \p sha1.
 * 
 * @param sha1[out] Must be SHA_DIGEST_LENGTH (20) in size.
 * @retval 0 Everything was okay.
 */
int sha1sum(FILE *f, unsigned char *sha1);
