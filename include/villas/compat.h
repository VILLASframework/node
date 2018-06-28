/** Compatability for different library versions.
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

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

#if JANSSON_VERSION_HEX < 0x020A00
size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __MACH__
  #include <libkern/OSByteOrder.h>

  #ifdef __cplusplus
  extern "C"{
  #endif

  #define le16toh(x) OSSwapLittleToHostInt16(x)
  #define le32toh(x) OSSwapLittleToHostInt32(x)
  #define le64toh(x) OSSwapLittleToHostInt64(x)
  #define be16toh(x) OSSwapBigToHostInt16(x)
  #define be32toh(x) OSSwapBigToHostInt32(x)
  #define be64toh(x) OSSwapBigToHostInt64(x)

  #define htole16(x) OSSwapHostToLittleInt16(x)
  #define htole32(x) OSSwapHostToLittleInt32(x)
  #define htole64(x) OSSwapHostToLittleInt64(x)
  #define htobe16(x) OSSwapHostToBigInt16(x)
  #define htobe32(x) OSSwapHostToBigInt32(x)
  #define htobe64(x) OSSwapHostToBigInt64(x)

  #ifdef __cplusplus
  }
  #endif
#endif /* __MACH__ */
