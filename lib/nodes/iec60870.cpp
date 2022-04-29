/** Node type: IEC60870-5-104
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

#include <villas/node_compat.hpp>
#include <villas/nodes/iec60870.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec60870;

// ------------------------------------------
// ASDUDataType
// ------------------------------------------

bool ASDUDataType::hasTimestamp() const {
	switch (this->type) {
	case ASDUDataType::SCALED:
	case ASDUDataType::NORMALIZED:
	case ASDUDataType::SHORT:
	case ASDUDataType::SINGLEPOINT:
	case ASDUDataType::DOUBLEPOINT:
		return false;
	case ASDUDataType::SCALED_WITH_TIMESTAMP:
	case ASDUDataType::NORMALIZED_WITH_TIMESTAMP:
	case ASDUDataType::SHORT_WITH_TIMESTAMP:
	case ASDUDataType::SINGLEPOINT_WITH_TIMESTAMP:
	case ASDUDataType::DOUBLEPOINT_WITH_TIMESTAMP:
		return true;
	default: assert(!"unreachable");
	}
}

ASDUDataType ASDUDataType::withoutTimestamp() const {
	switch (this->type) {
	case ASDUDataType::SCALED:
	case ASDUDataType::SCALED_WITH_TIMESTAMP:
		return ASDUDataType::SCALED;

	case ASDUDataType::NORMALIZED:
	case ASDUDataType::NORMALIZED_WITH_TIMESTAMP:
		return ASDUDataType::NORMALIZED;

	case ASDUDataType::SHORT:
	case ASDUDataType::SHORT_WITH_TIMESTAMP:
		return ASDUDataType::SHORT;

	case ASDUDataType::SINGLEPOINT:
	case ASDUDataType::SINGLEPOINT_WITH_TIMESTAMP:
		return ASDUDataType::SINGLEPOINT;

	case ASDUDataType::DOUBLEPOINT:
	case ASDUDataType::DOUBLEPOINT_WITH_TIMESTAMP:
		return ASDUDataType::DOUBLEPOINT;
	default: assert(!"unreachable");
	}
}

bool ASDUDataType::isConvertibleFromSignal(SignalType signal_type) const {
	switch (this->withoutTimestamp().type) {
	case ASDUDataType::SCALED:
	case ASDUDataType::NORMALIZED:
	case ASDUDataType::DOUBLEPOINT:
		return (signal_type == SignalType::INTEGER);
	case ASDUDataType::SINGLEPOINT:
		return (signal_type == SignalType::BOOLEAN);
	case ASDUDataType::SHORT:
		return (signal_type == SignalType::FLOAT);
	default: assert(!"unreachable");
	}
}

void ASDUDataType::addSignalsToASDU(
	CS101_ASDU &asdu,
	int ioa,
	QualityDescriptor quality,
	SignalType signal_type,
	SignalData* signal_data,
	unsigned signal_count,
	std::optional<timespec> timestamp
) const {
	assert(this->isConvertibleFromSignal(signal_type));

	// lib60870 allocates information objects internally
	// these optionals allow the allocations to be reused
	std::optional<MeasuredValueScaled> scaled;
	std::optional<MeasuredValueNormalized> normalized;
	std::optional<MeasuredValueShort> short_float;
	std::optional<SinglePointInformation> single_point;
	std::optional<DoublePointInformation> double_point;

	// the signal types are homogenous for now
	// this may be reevaluted later
	for (unsigned index = 0; index < signal_count; index++) {
		InformationObject io;
		switch (this->type) {
		case ASDUDataType::SCALED: {
			auto value = static_cast<int16_t> (signal_data[index].i & 0xFFFF);
			scaled = MeasuredValueScaled_create(scaled ? *scaled : NULL,ioa,value,quality);
			io = reinterpret_cast<InformationObject> (*scaled);
		} break;
		case ASDUDataType::NORMALIZED: {
			auto value = static_cast<int16_t> (signal_data[index].i & 0xFFFF);
			normalized = MeasuredValueNormalized_create(normalized ? *normalized : NULL,ioa,value,quality);
			io = reinterpret_cast<InformationObject> (*normalized);
		} break;
		case ASDUDataType::DOUBLEPOINT: {
			auto value = static_cast<DoublePointValue> (signal_data[index].i & 0x3);
			double_point = DoublePointInformation_create(double_point ? *double_point : NULL,ioa,value,quality);
			io = reinterpret_cast<InformationObject> (*double_point);
		} break;
		case ASDUDataType::SINGLEPOINT: {
			auto value = signal_data[index].b;
			single_point = SinglePointInformation_create(single_point ? *single_point : NULL,ioa,value,quality);
			io = reinterpret_cast<InformationObject> (*single_point);
		} break;
		case ASDUDataType::SHORT: {
			auto value = static_cast<float> (signal_data[index].f);
			short_float = MeasuredValueShort_create(short_float ? *short_float : NULL,ioa,value,quality);
			io = reinterpret_cast<InformationObject> (*short_float);
		} break;
		default: assert(!"unreachable");
		}
		CS101_ASDU_addInformationObject(asdu, io);
		InformationObject_destroy(io);
	}
}

ASDUDataType::ASDUDataType(ASDUDataType::Type t) : type(t) {}

ASDUDataType::operator ASDUDataType::Type() const {
	return this->type;
}

bool ASDUDataType::operator==(ASDUDataType v) const {
	return this->type == v.type;
}

bool ASDUDataType::operator!=(ASDUDataType v) const {
	return !(*this == v);
}

// ------------------------------------------
// TcpNode
// ------------------------------------------

TcpNode::TcpNode(const std::string &name) :
	Node(name)
{ }

TcpNode::~TcpNode()
{
	auto &server = this->server;

	if (server.created) {
		if (CS104_Slave_isRunning(server.slave)) {
			CS104_Slave_stop(server.slave);
		}
		CS104_Slave_destroy(server.slave);
	}
}

int TcpNode::_read(struct Sample *smps[], unsigned cnt)
{
	return 0;
}

int TcpNode::_write(struct Sample *smps[], unsigned cnt)
{
	return 0;
}

// ------------------------------------------
// Plugin
// ------------------------------------------

static char name[] = "IEC60870-5-104";
static char description[] = "Provide monitoring values over TCP/IP";
static NodePlugin<TcpNode, name, description, (int) NodeFactory::Flags::HIDDEN> p;
