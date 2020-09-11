/** Unit tests for sample value mapping.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/utils.hpp>
#include <villas/signal.h>

using namespace villas;

// cppcheck-suppress syntaxError
Test(mapping, parse_nodes)
{
	int ret;
	struct mapping_entry m;

	ret = mapping_entry_parse_str(&m, "apple.ts.origin");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "apple");
	cr_assert_eq(m.type, MappingType::TIMESTAMP);
	cr_assert_eq(m.timestamp.type, MappingTimestampType::ORIGIN);

	ret = mapping_entry_parse_str(&m, "cherry.stats.owd.mean");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "cherry");
	cr_assert_eq(m.type, MappingType::STATS);
	cr_assert_eq(m.stats.metric, Stats::Metric::OWD);
	cr_assert_eq(m.stats.type, Stats::Type::MEAN);

	ret = mapping_entry_parse_str(&m, "carrot.data[1-2]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "1");
	cr_assert_str_eq(m.data.last, "2");

	ret = mapping_entry_parse_str(&m, "carrot");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_eq(m.data.first, nullptr);
	cr_assert_eq(m.data.last, nullptr);

	ret = mapping_entry_parse_str(&m, "carrot.data[sole]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = mapping_entry_parse_str(&m, "carrot.sole");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = mapping_entry_parse_str(&m, "carrot.data.sole");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = mapping_entry_parse_str(&m, "carrot.data[sole-mio]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_str_eq(m.data.last, "mio");

	ret = mapping_entry_parse_str(&m, "carrot[sole-mio]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.node_name, "carrot");
	cr_assert_eq(m.type, MappingType::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_str_eq(m.data.last, "mio");
}
