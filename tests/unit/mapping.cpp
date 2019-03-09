/** Unit tests for sample value mapping.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
	struct vlist nodes = { .state = STATE_DESTROYED };

	const char *node_names[3] = { "apple", "cherry", "carrot" };
	const char *signal_names[3][4] = {
		{ "abra", "kadabra", "simsala", "bimm" },
		{ "this", "is", "a", "test" },
		{ "o",  "sole", "mio", "italia" }
	};

	vlist_init(&nodes);

	for (unsigned i = 0; i < ARRAY_LEN(node_names); i++) {
		struct node *n = new struct node;

		n->name = strdup(node_names[i]);
		n->in.signals.state = STATE_DESTROYED;

		vlist_init(&n->in.signals);

		for (unsigned j = 0; j < ARRAY_LEN(signal_names[i]); j++) {
			struct signal *sig;

			sig = signal_create(signal_names[i][j], nullptr, SIGNAL_TYPE_FLOAT);
			cr_assert_not_null(sig);

			vlist_push(&n->in.signals, sig);
		}

		vlist_push(&nodes, n);
	}

	ret = mapping_parse_str(&m, "apple.ts.origin", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "apple"));
	cr_assert_eq(m.type, MAPPING_TYPE_TIMESTAMP);
	cr_assert_eq(m.timestamp.type, MAPPING_TIMESTAMP_TYPE_ORIGIN);

	ret = mapping_parse_str(&m, "cherry.stats.owd.mean", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "cherry"));
	cr_assert_eq(m.type, MAPPING_TYPE_STATS);
	cr_assert_eq(m.stats.metric, STATS_METRIC_OWD);
	cr_assert_eq(m.stats.type, STATS_TYPE_MEAN);

	ret = mapping_parse_str(&m, "carrot.data[1-2]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = mapping_parse_str(&m, "carrot", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, -1);

	ret = mapping_parse_str(&m, "carrot.data[sole]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "carrot.data[sole-mio]", &nodes);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.node, vlist_lookup(&nodes, "carrot"));
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = vlist_destroy(&nodes, nullptr, true);
	cr_assert_eq(ret, 0);
}

Test(mapping, parse)
{
	int ret;
	struct mapping_entry m;

	ret = mapping_parse_str(&m, "ts.origin", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_TIMESTAMP);
	cr_assert_eq(m.timestamp.type, MAPPING_TIMESTAMP_TYPE_ORIGIN);

	ret = mapping_parse_str(&m, "hdr.sequence", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_HEADER);
	cr_assert_eq(m.header.type, MAPPING_HEADER_TYPE_SEQUENCE);

	ret = mapping_parse_str(&m, "stats.owd.mean", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_STATS);
	cr_assert_eq(m.stats.metric, STATS_METRIC_OWD);
	cr_assert_eq(m.stats.type, STATS_TYPE_MEAN);

	ret = mapping_parse_str(&m, "data[1-2]", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 1);
	cr_assert_eq(m.length, 2);

	ret = mapping_parse_str(&m, "data[5-5]", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 5);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "data[22]", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 22);
	cr_assert_eq(m.length, 1);

	ret = mapping_parse_str(&m, "data", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, -1);

	ret = mapping_parse_str(&m, "data[]", nullptr);
	cr_assert_eq(ret, 0);
	cr_assert_eq(m.type, MAPPING_TYPE_DATA);
	cr_assert_eq(m.data.offset, 0);
	cr_assert_eq(m.length, -1);

	ret = mapping_parse_str(&m, "data[1.1-2f]", nullptr);
	cr_assert_neq(ret, 0);

	/* Missing parts */
	ret = mapping_parse_str(&m, "stats.owd", nullptr);
	cr_assert_neq(ret, 0);

	/* This a type */
	ret = mapping_parse_str(&m, "hdr.sequences", nullptr);
	cr_assert_neq(ret, 0);

	/* Check for superfluous chars at the end */
	ret = mapping_parse_str(&m, "hdr.ts.origin.bla", nullptr);
	cr_assert_neq(ret, 0);

	ret = mapping_parse_str(&m, "hdr.ts.origin.", nullptr);
	cr_assert_neq(ret, 0);

	ret = mapping_parse_str(&m, "data[1-2]bla", nullptr);
	cr_assert_neq(ret, 0);

	/* Negative length of chunk */
	ret = mapping_parse_str(&m, "data[5-3]", nullptr);
	cr_assert_neq(ret, 0);
}
