/** Unit tests for sample value mapping.
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

#include <criterion/criterion.h>

#include "mapping.h"
#include "node.h"
#include "list.h"

Test(mapping, parse_nodes)
{
	int ret;
	struct mapping_entry m;
	struct list n = { .state = STATE_DESTROYED };

	struct node n1 = { .name = "apple" };
	struct node n2 = { .name = "cherry" };
	struct node n3 = { .name = "carrot" };

	list_init(&n);

	list_push(&n, &n1);
	list_push(&n, &n2);
	list_push(&n, &n3);

	ret = mapping_parse_str(&m, "apple.ts.origin", &n);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, &n1);
	cr_assert_eq(m.type, MAPPING_TYPE_TS);
	cr_assert_eq(m.ts.id, MAPPING_TS_ORIGIN);

	ret = mapping_parse_str(&m, "cherry.stats.owd.mean", &n);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, &n2);
	cr_assert_eq(m.type, MAPPING_TYPE_STATS);
	cr_assert_eq(m.stats.id, STATS_OWD);
	cr_assert_eq(m.stats.type, MAPPING_STATS_TYPE_MEAN);

	ret = mapping_parse_str(&m, "carrot.data[1-2]", &n);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, &n3);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = mapping_parse_str(&m, "carrot", &n);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, &n3);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, 0);

	ret = list_destroy(&n, NULL, false);
	cr_assert_eq(ret, 0);
}

Test(mapping, parse)
{
	int ret;
	struct mapping_entry m;

	ret = mapping_parse_str(&m, "ts.origin", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_TS);
	cr_assert_eq(m.ts.id, MAPPING_TS_ORIGIN);

	ret = mapping_parse_str(&m, "hdr.sequence", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_HDR);
	cr_assert_eq(m.hdr.id, MAPPING_HDR_SEQUENCE);

	ret = mapping_parse_str(&m, "stats.owd.mean", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_STATS);
	cr_assert_eq(m.stats.id, STATS_OWD);
	cr_assert_eq(m.stats.type, MAPPING_STATS_TYPE_MEAN);

	ret = mapping_parse_str(&m, "data[1-2]", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = mapping_parse_str(&m, "data[5-5]", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 5);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "data[22]", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 22);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "data", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, 0);

	ret = mapping_parse_str(&m, "data[]", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, 0);

	ret = mapping_parse_str(&m, "data[1.1-2f]", NULL);
	cr_assert_neq(ret, 0);

	/* Missing parts */
	ret = mapping_parse_str(&m, "stats.owd", NULL);
	cr_assert_neq(ret, 0);

	/* This a type */
	ret = mapping_parse_str(&m, "hdr.sequences", NULL);
	cr_assert_neq(ret, 0);

	/* Check for superfluous chars at the end */
	ret = mapping_parse_str(&m, "stats.ts.origin.bla", NULL);
	cr_assert_neq(ret, 0);

	ret = mapping_parse_str(&m, "stats.ts.origin.", NULL);
	cr_assert_neq(ret, 0);

	ret = mapping_parse_str(&m, "data[1-2]bla", NULL);
	cr_assert_neq(ret, 0);

	/* Negative length of chunk */
	ret = mapping_parse_str(&m, "data[5-3]", NULL);
	cr_assert_eq(ret, -1);
}
