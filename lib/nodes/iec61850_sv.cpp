/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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
#include <pthread.h>
#include <unistd.h>

#include <villas/sample.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/iec61850_sv.hpp>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::node::iec61850;

SVNode::SVNode(const std::string &name) :
	Node(name),
	app_id(CONFIG_SV_DEFAULT_APPID)
{
	out.enabled = false;
	in.enabled = false;

	out.publisher = nullptr;
	in.subscriber = nullptr;

	out.smpmod = -1; /* do not set smpmod */
	out.smprate = -1; /* do not set smpmod */
	out.confrev = 1;
	out.vlan_priority = CONFIG_SV_DEFAULT_PRIORITY;
	out.vlan_id = CONFIG_SV_DEFAULT_VLAN_ID;

	uint8_t tmp[] = CONFIG_SV_DEFAULT_DST_ADDRESS;
	memcpy(dst_address.ether_addr_octet, tmp, sizeof(dst_address.ether_addr_octet));
}

SVNode::~SVNode()
{
	int ret __attribute__((unused));

	/* Deinitialize publisher */
	if (out.publisher)
		SVPublisher_destroy(out.publisher);

	if (in.subscriber)
		SVSubscriber_destroy(in.subscriber);

	if (in.enabled) {
		ret = queue_signalled_destroy(&in.queue);
		ret = pool_destroy(&in.pool);
	}
}

void SVNode::listenerStatic(SVSubscriber subscriber, void *ctx, SVSubscriber_ASDU asdu)
{
	auto *n = static_cast<SVNode *>(ctx);
	n->listener(subscriber, asdu);
}

void SVNode::listener(SVSubscriber subscriber, SVSubscriber_ASDU asdu)
{
	struct Sample *smp;

	const char* svid = SVSubscriber_ASDU_getSvId(asdu);
	int smpcnt       = SVSubscriber_ASDU_getSmpCnt(asdu);
	int confrev      = SVSubscriber_ASDU_getConfRev(asdu);
	int sz;

	logger->debug("Received SV: svid={}, smpcnt={}, confrev={}", svid, smpcnt, confrev);

	sz = SVSubscriber_ASDU_getDataSize(asdu);
	if (sz < in.total_size) {
		logger->warn("Received truncated ASDU: size={}, expected={}", SVSubscriber_ASDU_getDataSize(asdu), in.total_size);
		return;
	}

	/* Access to the data requires a priori knowledge of the data set.
	 * For this example we assume a data set consisting of FLOAT32 values.
	 * A FLOAT32 value is encoded as 4 bytes. You can find the first FLOAT32
	 * value at byte position 0, the second value at byte position 4, the third
	 * value at byte position 8, and so on.
	 *
	 * To prevent damages due configuration, please check the length of the
	 * data block of the SV message before accessing the data.
	 */

	smp = sample_alloc(&in.pool);
	if (!smp) {
		logger->warn("Pool underrun in subscriber");
		return;
	}

	smp->sequence = smpcnt;
	smp->flags = (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;
	smp->length = 0;
	smp->signals = getInputSignals(false);

	if (SVSubscriber_ASDU_hasRefrTm(asdu)) {
		uint64_t refrtm = SVSubscriber_ASDU_getRefrTmAsMs(asdu);

		smp->ts.origin.tv_sec = refrtm / 1000;
		smp->ts.origin.tv_nsec = (refrtm % 1000) * 1000000;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}

	unsigned offset = 0;
	for (size_t j = 0; j < in.signals.size(); j++) {
		auto *td = in.signals[j];
		auto sig = smp->signals->getByIndex(j);
		if (!sig)
			continue;

		switch (td->iec_type) {
			case Type::INT8:
				smp->data[j].i = SVSubscriber_ASDU_getINT8(asdu,    offset);
				break;

			case Type::INT16:
				smp->data[j].i = SVSubscriber_ASDU_getINT16(asdu,   offset);
				break;

			case Type::INT32:
				smp->data[j].i = SVSubscriber_ASDU_getINT32(asdu,   offset);
				break;

			case Type::INT8U:
				smp->data[j].i = SVSubscriber_ASDU_getINT8U(asdu,   offset);
				break;

			case Type::INT16U:
				smp->data[j].i = SVSubscriber_ASDU_getINT16U(asdu,  offset);
				break;

			case Type::INT32U:
				smp->data[j].i = SVSubscriber_ASDU_getINT32U(asdu,  offset);
				break;

			case Type::FLOAT32:
				smp->data[j].f = SVSubscriber_ASDU_getFLOAT32(asdu, offset);
				break;

			case Type::FLOAT64:
				smp->data[j].f = SVSubscriber_ASDU_getFLOAT64(asdu, offset);
				break;

			default: { }
		}

		offset += td->size;

		smp->length++;
	}

	int pushed = queue_signalled_push(&in.queue, smp);
	if (pushed != 1)
		logger->warn("Queue overrun");
}

int SVNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	const char *daddr = nullptr;
	const char *intf = nullptr;
	const char *svid = nullptr;
	const char *smpmod = nullptr;

	json_t *json_in = nullptr;
	json_t *json_out = nullptr;
	json_t *json_signals = nullptr;
	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s: s, s?: i, s?: s }",
		"out", &json_out,
		"in", &json_in,
		"interface", &intf,
		"app_id", &app_id,
		"dst_address", &daddr
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-iec61850-sv");

	if (intf)
		interface = intf;

	if (daddr)
		ether_aton_r(daddr, &dst_address);

	if (json_out) {
		out.enabled = true;

		ret = json_unpack_ex(json_out, &err, 0, "{ s: o, s: s, s?: i, s?: s, s?: i, s?: i, s?: i }",
			"signals", &json_signals,
			"svid", &svid,
			"confrev", &out.confrev,
			"smpmod", &smpmod,
			"smprate", &out.smprate,
			"vlan_id", &out.vlan_id,
			"vlan_priority", &out.vlan_priority
		);
		if (ret)
			throw ConfigError(json_out, err, "node-config-node-iec61850-sv-out");

		if (smpmod) {
			if      (!strcmp(smpmod, "per_nominal_period"))
				out.smpmod = IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD;
			else if (!strcmp(smpmod, "samples_per_second"))
				out.smpmod = IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND;
			else if (!strcmp(smpmod, "seconds_per_sample"))
				out.smpmod = IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE;
			else
				throw RuntimeError("Invalid value '{}' for setting 'smpmod'", smpmod);
		}

		if (svid)
			out.svid = svid;

		ret = parseSignals(json_signals, out.signals, getOutputSignals());
		if (ret <= 0)
			throw RuntimeError("Failed to parse output signal configuration");

		out.total_size = ret;
	}

	if (json_in) {
		in.enabled = true;

		json_signals = nullptr;
		ret = json_unpack_ex(json_in, &err, 0, "{ s: o }",
			"signals", &json_signals
		);
		if (ret)
			throw ConfigError(json_in, err, "node-config-node-iec61850-in");

		ret = parseSignals(json_signals, in.signals, getInputSignals(false));
		if (ret <= 0)
			throw RuntimeError("Failed to parse input signal configuration");

		in.total_size = ret;
	}

	return 0;
}

std::string SVNode::getDetails()
{
	auto details = fmt::format("interface={}, app_id={:#x}, dst_address={}", interface, app_id, ether_ntoa(&dst_address));

	/* Publisher part */
	if (out.enabled) {
		details += fmt::format(", pub.svid={}, pub.vlan_prio={}, pub.vlan_id={:#x}, pub.confrev={}",
			out.svid,
			out.vlan_priority,
			out.vlan_id,
			out.confrev
		);
	}

	return details;
}

int SVNode::start()
{
	int ret;

	/* Initialize publisher */
	if (out.enabled) {
		out.publisher = SVPublisher_create(nullptr, interface.c_str());
		out.asdu = SVPublisher_addASDU(out.publisher, out.svid.c_str(), getNameShort().c_str(), out.confrev);

		for (auto *td : out.signals) {
			switch (td->iec_type) {
				case Type::INT8:
					SVPublisher_ASDU_addINT8(out.asdu);
					break;

				case Type::INT32:
					SVPublisher_ASDU_addINT32(out.asdu);
					break;

				case Type::FLOAT32:
					SVPublisher_ASDU_addFLOAT(out.asdu);
					break;

				case Type::FLOAT64:
					SVPublisher_ASDU_addFLOAT64(out.asdu);
					break;

				default:
					logger->warn("Skipping unsupported data-type in SV publisher: {}", td->name);
			}
		}

		if (out.smpmod >= 0)
			SVPublisher_ASDU_setSmpMod(out.asdu, out.smpmod);

		SVPublisher_ASDU_enableRefrTm(out.asdu);

// TODO: figure out why this is commented...
//		if (s->out.smprate >= 0)
//			SV_ASDU_setSmpRate(out.asdu, out.smprate);

		/* Start publisher */
		SVPublisher_setupComplete(out.publisher);
	}

	/* Start subscriber */
	if (in.enabled) {
		in.receiver = new SVReceiver(interface);
		in.subscriber = SVSubscriber_create(dst_address.ether_addr_octet, app_id);

		/* Install a callback handler for the subscriber */
		SVSubscriber_setListener(in.subscriber, listenerStatic, this);

		/* Connect the subscriber to the receiver */
		in.receiver->addSubscriber(in.subscriber);

		/* Initialize pool and queue to pass samples between threads */
		// TODO: Make pool & queue size configurable
		ret = pool_init(&in.pool, 1024, SAMPLE_LENGTH(getInputSignals(false)->size()));
		if (ret)
			return ret;

		ret = queue_signalled_init(&in.queue, 1024);
		if (ret)
			return ret;
	}

	return Node::start();
}

int SVNode::stop()
{
	int ret = Node::stop();
	if (ret)
		return ret;

	if (in.enabled)
		in.receiver->removeSubscriber(in.subscriber);

	ret = queue_signalled_close(&in.queue);
	if (ret)
		return ret;

	return 0;
}

int SVNode::_read(struct Sample *smps[], unsigned cnt)
{
	int pulled;
	struct Sample *smpt[cnt];

	if (!in.enabled)
		return -1;

	pulled = queue_signalled_pull_many(&in.queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);
	sample_decref_many(smpt, pulled);

	return pulled;
}

int SVNode::_write(struct Sample *smps[], unsigned cnt)
{
	if (!out.enabled)
		return -1;

	for (unsigned j = 0; j < cnt; j++) {
		unsigned offset = 0;
		for (unsigned k = 0; k < MIN(smps[j]->length, out.signals.size()); k++) {
			auto *td = out.signals[k];

			int ival = 0;
			double fval = 0;

			switch (td->iec_type) {
				case Type::INT8:
				case Type::INT32:
					ival = sample_format(smps[j], k) == SignalType::FLOAT ? smps[j]->data[k].f : smps[j]->data[k].i;
					break;

				case Type::FLOAT32:
				case Type::FLOAT64:
					fval = sample_format(smps[j], k) == SignalType::FLOAT ? smps[j]->data[k].f : smps[j]->data[k].i;
					break;

				default: { }
			}

			switch (td->iec_type) {
				case Type::INT8:
					SVPublisher_ASDU_setINT8(out.asdu,    offset, ival);
					break;

				case Type::INT32:
					SVPublisher_ASDU_setINT32(out.asdu,   offset, ival);
					break;

				case Type::FLOAT32:
					SVPublisher_ASDU_setFLOAT(out.asdu,   offset, fval);
					break;

				case Type::FLOAT64:
					SVPublisher_ASDU_setFLOAT64(out.asdu, offset, fval);
					break;

				default: { }
			}

			offset += td->size;
		}

		SVPublisher_ASDU_setSmpCnt(out.asdu, smps[j]->sequence);

		if (smps[j]->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
			uint64_t refrtm = smps[j]->ts.origin.tv_sec * 1000 + smps[j]->ts.origin.tv_nsec / 1000000;

			SVPublisher_ASDU_setRefrTm(out.asdu, refrtm);
		}

		SVPublisher_publish(out.publisher);
	}

	return cnt;
}

std::vector<int> SVNode::getPollFDs()
{
	return { queue_signalled_fd(&in.queue) };
}

static char n[] = "iec61850-9-2";
static char d[] = "IEC 61850-9-2 (Sampled Values)";
static NodePlugin<SVNode, n, d, (int) NodeFactory::Flags::SUPPORTS_POLL |
                                (int) NodeFactory::Flags::SUPPORTS_READ |
                                (int) NodeFactory::Flags::SUPPORTS_WRITE> nf;
