/** Print fancy tables
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

/** @addtogroup table Print fancy tables
 * @{
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct table_column {
	int width;	/**< Width of the column. */
	char *title;	/**< The title as shown in the table header. */
	char *format;	/**< The format which is used to print the table rows. */
	char *unit;	/**< An optional unit which will be shown in the table header. */

	enum {
		TABLE_ALIGN_LEFT,
		TABLE_ALIGN_RIGHT
	} align;

	int _width;	/**< The real width of this column. Calculated by table_header() */
};

struct table {
	int ncols;
	int width;
	struct table_column *cols;
};

/** Print a table header consisting of \p n columns. */
void table_header(struct table *t);

/** Print table rows. */
void table_row(struct table *t, ...);

/** Print the table footer. */
void table_footer(struct table *t);

/** @} */

#ifdef __cplusplus
}
#endif
