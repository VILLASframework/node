/** UltraLight 2.0 format as used by FIWARE IotAgent.
 *
 * See: https://fiware-iotagent-ul.readthedocs.io/en/latest/usermanual/index.html
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
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

#include <cstring>

#include <villas/sample.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/compat.hpp>
#include <villas/timing.h>
#include <villas/formats/iotagent_ul.hpp>

using namespace villas;
using namespace villas::node;

int IotAgentUltraLightFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct sample * const smps[], unsigned cnt)
{
	size_t printed = 0;
	struct signal *sig;
	const struct sample *smp = smps[0];

	for (unsigned i = 0; (i < smp->length) && (printed < len); i++) {
		sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			return -1;

		if (sig->name)
			printed += snprintf(buf + printed, len - printed, "%s|%f|", sig->name, smp->data[i].f);
		else {
			char name[32];
			snprintf(name, 32, "signal_%u", i);
			printed += snprintf(buf + printed, len - printed, "%s|%f|", name, smp->data[i].f);
		}
	}

	if (wbytes)
		*wbytes = printed - 1; // -1 to cut off last '|'

	return 0;
}

int IotAgentUltraLightFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct sample * const smps[], unsigned cnt)
{
	return -1;
}

static char n[] = "iotagent_ul";
static char d[] = "FIWARE IotAgent UltraLight format";
static FormatPlugin<IotAgentUltraLightFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
