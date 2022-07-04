/** Unit tests for sample value mapping.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
