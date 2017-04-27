/** Vendor, Library, Name, Version (VLNV) tag.
 *
 * @file
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#ifndef _FPGA_VLNV_H_
#define _FPGA_VLNV_H_

/* Forward declarations */
struct list;

struct fpga_vlnv {
	char *vendor;
	char *library;
	char *name;
	char *version;
};

/** Return the first IP block in list \p l which matches the VLNV */
struct fpga_ip * fpga_vlnv_lookup(struct list *l, struct fpga_vlnv *v);

/** Check if IP block \p c matched VLNV. */
int fpga_vlnv_cmp(struct fpga_vlnv *a, struct fpga_vlnv *b);

/** Tokenizes VLNV \p vlnv and stores it into \p c */
int fpga_vlnv_parse(struct fpga_vlnv *c, const char *vlnv);

/** Release memory allocated by fpga_vlnv_parse(). */
int fpga_vlnv_destroy(struct fpga_vlnv *v);

#endif /** _FPGA_VLNV_H_ @} */