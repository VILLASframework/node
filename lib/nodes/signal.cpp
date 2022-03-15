/** Node-type for signal generation.
 *
 * @file
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

#include <list>
#include <cmath>
#include <cstring>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/signal.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SignalNodeSignal::SignalNodeSignal(json_t *json) :
	type(Type::MIXED),
	frequency(1.0),
	amplitude(1.0),
	stddev(0.2),
	offset(0.0),
	pulse_width(1.0),
	pulse_low(0.0),
	pulse_high(1.0),
	phase(0.0)
{
	parse(json);

	last = offset;
}

enum SignalNodeSignal::Type SignalNodeSignal::lookupType(const std::string &type)
{
	if      (type == "random")
		return Type::RANDOM;
	else if (type == "sine")
		return Type::SINE;
	else if (type == "square")
		return Type::SQUARE;
	else if (type == "triangle")
		return Type::TRIANGLE;
	else if (type == "ramp")
		return Type::RAMP;
	else if (type == "counter")
		return Type::COUNTER;
	else if (type == "constant")
		return Type::CONSTANT;
	else if (type == "mixed")
		return Type::MIXED;
	else if (type == "pulse")
		return Type::PULSE;

	throw std::invalid_argument("Invalid signal type");
}

std::string SignalNodeSignal::typeToString(enum Type type)
{
	switch (type) {
		case Type::CONSTANT:
			return "constant";

		case Type::SINE:
			return "sine";

		case Type::TRIANGLE:
			return "triangle";

		case Type::SQUARE:
			return "square";

		case Type::RAMP:
			return "ramp";

		case Type::COUNTER:
			return "counter";

		case Type::RANDOM:
			return "random";

		case Type::MIXED:
			return "mixed";

		case Type::PULSE:
			return "pulse";

		default:
			return nullptr;
	}
}

void SignalNodeSignal::start()
{
	last = offset;

}

void SignalNodeSignal::read(unsigned c, double t, double r, SignalData *d)
{
	switch (type) {
		case Type::CONSTANT:
			d->f = offset + amplitude;
			break;

		case Type::SINE:
			d->f = offset + amplitude *        sin(t * frequency * 2 * M_PI + phase);
			break;

		case Type::TRIANGLE:
			d->f = offset + amplitude * (fabs(fmod(t * frequency + (phase / (2 * M_PI)), 1) - .5) - 0.25) * 4;
			break;

		case Type::SQUARE:
			d->f = offset + amplitude * (    (fmod(t * frequency + (phase / (2 * M_PI)), 1) < .5) ? -1 : 1);
			break;

		case Type::RAMP:
			d->f = offset + amplitude *       fmod(t, frequency);
			break;

		case Type::COUNTER:
			d->f = offset + amplitude * c;
			break;

		case Type::RANDOM:
			last += boxMuller(0, stddev);
			d->f = last;
			break;

		case Type::MIXED:
			break;

		case Type::PULSE:
			d->f = abs(fmod(t * frequency + (phase / (2 * M_PI)) , 1)) <= (pulse_width / r)
					? pulse_high
					: pulse_low;
			d->f += offset;
			break;
	}
}

int SignalNodeSignal::parse(json_t *json)
{
	int ret;
	const char *type_str;

	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: F, s?: F, s?: F, s?: F, s?: F, s?: F, s?: F, s?: F }",
		"signal", &type_str,
		"frequency", &frequency,
		"amplitude", &amplitude,
		"stddev", &stddev,
		"offset", &offset,
		"pulse_width", &pulse_width,
		"pulse_low", &pulse_low,
		"pulse_high", &pulse_high,
		"phase", &phase
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-signal");

	try {
		type = lookupType(type_str);
	} catch (std::invalid_argument &e) {
		throw ConfigError(json, "node-config-node-signal-type", "Unknown signal type: {}", type_str);
	}

	return 0;
}

Signal::Ptr SignalNodeSignal::toSignal(Signal::Ptr tpl) const
{
	auto isDefaultName = tpl->name.rfind("signal", 0) == 0;

	auto name = isDefaultName ? typeToString(type) : tpl->name;
	auto unit = tpl->unit;

	auto sig = std::make_shared<Signal>(name, unit, SignalType::FLOAT);

	sig->init.f = offset;

	return sig;
}

SignalNode::SignalNode(const std::string &name) :
	Node(name),
	task(),
	rt(1),
	rate(10),
	monitor_missed(true),
	limit(-1),
	missed_steps(0)
{ }

int SignalNode::prepare()
{
	assert(state == State::CHECKED);

	for (unsigned i = 0; i < signals.size(); i++) {
		auto &sig = (*in.signals)[i];
		auto &ssig = signals[i];

		sig = ssig.toSignal(sig);
	}

	if (logger->level() <= spdlog::level::debug)
		in.signals->dump(logger);

	return Node::prepare();
}

int SignalNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int r = -1, m = -1, ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_error_t err;

	size_t i;
	json_t *json_signals, *json_signal;

	ret = json_unpack_ex(json, &err, 0, "{ s?: b, s?: i, s?: F, s?: b, s: { s: o } }",
		"realtime", &r,
		"limit", &limit,
		"rate", &rate,
		"monitor_missed", &m,
		"in",
			"signals", &json_signals
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-signal");

	if (r >= 0)
		rt = r != 0;

	if (m >= 0)
		monitor_missed = m != 0;

	signals.clear();
	unsigned j = 0;
	json_array_foreach(json_signals, i, json_signal) {
		auto sig = SignalNodeSignal(json_signal);

		if (sig.type == SignalNodeSignal::Type::MIXED)
			sig.type = (SignalNodeSignal::Type) (j++ % (int) SignalNodeSignal::Type::MIXED);

		signals.push_back(sig);
	}

	state = State::PARSED;

	return 0;
}

int SignalNode::start()
{
	assert(state == State::PREPARED ||
	       state == State::PAUSED);

	missed_steps = 0;
	started = time_now();

	for (auto sig : signals)
		sig.start();

	/* Setup task */
	if (rt)
		task.setRate(rate);

	int ret = Node::start();
	if (!ret)
		state = State::STARTED;

	return ret;
}

int SignalNode::stop()
{
	assert(state == State::STARTED ||
	       state == State::PAUSED ||
	       state == State::STOPPING);

	int ret = Node::stop();
	if (ret)
		return ret;

	if (rt)
		task.stop();

	if (missed_steps > 0 && monitor_missed)
		logger->warn("Missed a total of {} steps.", missed_steps);

	state = State::STOPPED;

	return 0;
}

int SignalNode::_read(struct Sample *smps[], unsigned cnt)
{
	struct Sample *t = smps[0];

	struct timespec ts;
	uint64_t steps, counter = sequence - sequence_init;

	assert(cnt == 1);

	if (rt)
		ts = time_now();
	else {
		struct timespec offset = time_from_double(counter * 1.0 / rate);
		ts = time_add(&started, &offset);
	}

	double running = time_delta(&started, &ts);

	t->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE;
	t->ts.origin = ts;
	t->sequence = sequence;
	t->length = MIN(signals.size(), t->capacity);
	t->signals = in.signals;

	for (unsigned i = 0; i < t->length; i++) {
		auto &sig = signals[i];

		sig.read(counter, running, rate, &t->data[i]);
	}

	if (limit > 0 && counter >= (unsigned) limit) {
		logger->info("Reached limit.");

		setState(State::STOPPING);
		return -1;
	}

	/* Throttle output if desired */
	if (rt) {
		/* Block until 1/p->rate seconds elapsed */
		steps = task.wait();
		if (steps > 1 && monitor_missed) {
			logger->debug("Missed steps: {}", steps-1);
			missed_steps += steps-1;
		}
	}
	else
		steps = 1;

	sequence += steps;

	return 1;
}

const std::string & SignalNode::getDetails()
{
	if (details.empty()) {
		details = fmt::format("rt={}, rate={}", rt ? "yes" : "no", rate);

		if (limit > 0)
			details += fmt::format(", limit={}", limit);

	}

	return details;
}

std::vector<int> SignalNode::getPollFDs()
{
	if (rt)
		return { task.getFD() };

	return {};
}

static char n[] = "signal.v2";
static char d[] = "Signal generator";
static NodePlugin<SignalNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_POLL> p;
