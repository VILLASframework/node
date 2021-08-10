/** Unit tests for sample value mapping.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/mapping.hpp>
#include <villas/node.hpp>
#include <villas/list.hpp>
#include <villas/utils.hpp>
#include <villas/signal.hpp>

using namespace villas;
using namespace villas::node;

// cppcheck-suppress syntaxError
Test(mapping, parse_nodes)
{
	int ret;
	MappingEntry m;

	ret = m.parseString("apple.ts.origin");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "apple");
	cr_assert_eq(m.type, MappingEntry::Type::TIMESTAMP);
	cr_assert_eq(m.timestamp.type, MappingEntry::TimestampType::ORIGIN);

	ret = m.parseString("cherry.stats.owd.mean");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "cherry");
	cr_assert_eq(m.type, MappingEntry::Type::STATS);
	cr_assert_eq(m.stats.metric, Stats::Metric::OWD);
	cr_assert_eq(m.stats.type, Stats::Type::MEAN);

	ret = m.parseString("carrot.data[1-2]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "1");
	cr_assert_str_eq(m.data.last, "2");

	ret = m.parseString("carrot");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_eq(m.data.first, nullptr);
	cr_assert_eq(m.data.last, nullptr);

	ret = m.parseString("carrot.data[sole]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = m.parseString("carrot.sole");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = m.parseString("carrot.data.sole");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_eq(m.data.last, nullptr);

	ret = m.parseString("carrot.data[sole-mio]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_str_eq(m.data.last, "mio");

	ret = m.parseString("carrot[sole-mio]");
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(m.nodeName.c_str(), "carrot");
	cr_assert_eq(m.type, MappingEntry::Type::DATA);
	cr_assert_str_eq(m.data.first, "sole");
	cr_assert_str_eq(m.data.last, "mio");
}
