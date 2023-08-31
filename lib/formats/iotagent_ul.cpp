/* UltraLight 2.0 format as used by FIWARE IotAgent.
 *
 * See: https://fiware-iotagent-ul.readthedocs.io/en/latest/usermanual/index.html
 *
 * Author: Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/sample.hpp>
#include <villas/node.hpp>
#include <villas/signal.hpp>
#include <villas/compat.hpp>
#include <villas/timing.hpp>
#include <villas/formats/iotagent_ul.hpp>

using namespace villas;
using namespace villas::node;

int IotAgentUltraLightFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt)
{
	size_t printed = 0;
	const struct Sample *smp = smps[0];

	for (unsigned i = 0; (i < smp->length) && (printed < len); i++) {
		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			return -1;

		if (!sig->name.empty())
			printed += snprintf(buf + printed, len - printed, "%s|%f|", sig->name.c_str(), smp->data[i].f);
		else {
			printed += snprintf(buf + printed, len - printed, "signal_%u|%f|", i, smp->data[i].f);
		}
	}

	if (wbytes)
		*wbytes = printed - 1; // -1 to cut off last '|'

	return 0;
}

int IotAgentUltraLightFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt)
{
	return -1;
}

// Register format
static char n[] = "iotagent_ul";
static char d[] = "FIWARE IotAgent UltraLight format";
static FormatPlugin<IotAgentUltraLightFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
