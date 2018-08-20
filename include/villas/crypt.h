/** Crypto helpers.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <stdio.h>
#include <openssl/sha.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Calculate SHA1 hash of complete file \p f and place it into \p sha1.
 *
 * @param sha1[out] Must be SHA_DIGEST_LENGTH (20) in size.
 * @retval 0 Everything was okay.
 */
int sha1sum(FILE *f, unsigned char *sha1);

#ifdef __cplusplus
}
#endif