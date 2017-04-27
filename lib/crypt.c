/** Crypto helpers.
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

#include "crypt.h"

int sha1sum(FILE *f, unsigned char *sha1)
{
	SHA_CTX c;
	char buf[512];
	ssize_t bytes;
	long seek;
	
	seek = ftell(f);
	fseek(f, 0, SEEK_SET);

	SHA1_Init(&c);

	bytes = fread(buf, 1, 512, f);
	while (bytes > 0) {
		SHA1_Update(&c, buf, bytes);
		bytes = fread(buf, 1, 512, f);
	}

	SHA1_Final(sha1, &c);
	
	fseek(f, seek, SEEK_SET);

	return 0;
}

