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

#include <villas/mapping.h>
#include <villas/node.h>
#include <villas/list.h>
#include <villas/utils.h>
#include <villas/signal.h>

Test(mapping, parse_nodes)
{
	int ret;
	struct mapping_entry m;
	struct list nodes = { .state = STATE_DESTROYED };

	char *node_names[3] = { "apple", "cherry", "carrot" };
	char *signal_names[3][4] = {
		{ "abra", "kadabra", "simsala", "bimm" },
		{ "this", "is", "a", "test" },
		{ "o",  "sole", "mio", "italia" }
	};

	list_init(&nodes);

	for (int i = 0; i < ARRAY_LEN(node_names); i++) {
		struct node *n = alloc(sizeof(struct node));

		n->name = node_names[i];
		n->in.signals.state = STATE_DESTROYED;

		list_init(&n->in.signals);

		for (int j = 0; j < ARRAY_LEN(signal_names[i]); j++) {
			struct signal *s = alloc(sizeof(struct signal *));

			s->name = signal_names[i][j];

			list_push(&n->in.signals, s);
		}

		list_push(&nodes, n);
	}

	ret = mapping_parse_str(&m, "apple.ts.origin", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "apple"));
	cr_assert_eq(m.type, MAPPING_TYPE_TIMESTAMP);
	cr_assert_eq(m.ts.id, MAPPING_TS_ORIGIN);

	ret = mapping_parse_str(&m, "cherry.stats.owd.mean", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "cherry"));
	cr_assert_eq(m.type, MAPPING_TYPE_STATS);
	cr_assert_eq(m.stats.id, STATS_OWD);
	cr_assert_eq(m.stats.type, MAPPING_STATS_TYPE_MEAN);

	ret = mapping_parse_str(&m, "carrot.data[1-2]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = mapping_parse_str(&m, "carrot", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, 0);

	ret = mapping_parse_str(&m, "carrot.data[sole]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "carrot.data[sole-mio]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, list_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = list_destroy(&nodes, NULL, true);
	cr_assert_eq(ret, 0);
}

Test(mapping, parse)
{
	int ret;
	struct mapping_entry m;

	ret = mapping_parse_str(&m, "ts.origin", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_TIMESTAMP);
	cr_assert_eq(m.ts.id, MAPPING_TS_ORIGIN);

	ret = mapping_parse_str(&m, "hdr.sequence", NULL);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_HEADER);
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
