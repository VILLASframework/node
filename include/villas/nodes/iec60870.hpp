/** Node type: IEC60870-5-104
 *
 * @file
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

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <ctime>
#include <array>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/signal.hpp>
#include <lib60870/iec60870_common.h>
#include <lib60870/cs101_information_objects.h>
#include <lib60870/cs104_slave.h>

namespace villas {
namespace node {
namespace iec60870 {

// A supported CS101 information data type
class ASDUData {
public:
	enum Type {
		// MeasuredValueScaled
		// 16 bit
		SCALED = M_ME_NB_1,
		// MeasuredValueScaled + Timestamp
		// 16 bit
		SCALED_WITH_TIMESTAMP = M_ME_TE_1,
		// SinglePoint
		// bool
		SINGLEPOINT = M_SP_NA_1,
		// SinglePoint + Timestamp
		// bool + timestamp
		SINGLEPOINT_WITH_TIMESTAMP = M_SP_TB_1,
		// DoublePoint
		// 2 bit enum
		DOUBLEPOINT = M_DP_NA_1,
		// DoublePoint + Timestamp
		// 2 bit enum + timestamp
		DOUBLEPOINT_WITH_TIMESTAMP = M_DP_TB_1,
		// MeasuredValueNormalized
		// 16 bit
		NORMALIZED = M_ME_NA_1,
		// MeasuredValueNormalized + Timestamp
		// 16 bit + timestamp
		NORMALIZED_WITH_TIMESTAMP = M_ME_TD_1,
		// MeasuredValueShort
		// float
		SHORT = M_ME_NC_1,
		// MeasuredValueShort + Timestamp
		// float + timestamp
		SHORT_WITH_TIMESTAMP = M_ME_TF_1,
	};

	struct Sample {
		SignalData signal_data;
		QualityDescriptor quality;
		std::optional<timespec> timestamp;
	};

	// lookup datatype for config name
	static std::optional<ASDUData> lookupName(char const* name, bool with_timestamp, int ioa);
	// lookup datatype for numeric type
	static std::optional<ASDUData> lookupType(int type, int ioa);

	// does this data include a timestamp
	bool hasTimestamp() const;
	// the IEC104 type
	ASDUData::Type type() const;
	// the config file identifier for this type
	char const* name() const;
	// get equivalent IEC104 type without timestamp (e.g. for general interrogation response)
	ASDUData::Type typeWithoutTimestamp() const;
	// corresponding signal type
	SignalType signalType() const;
	// check if ASDU contains this data
	std::optional<ASDUData::Sample> checkASDU(CS101_ASDU const &asdu) const;
	// add SignalData to an ASDU
	void addSampleToASDU(CS101_ASDU &asdu, ASDUData::Sample sample) const;

	// every value in an ASDU has an associated "information object address" (ioa)
	int ioa;
private:
	struct Descriptor {
		ASDUData::Type type;
		char const *name;
		bool has_timestamp;
		ASDUData::Type type_without_timestamp;
		SignalType signal_type;
	};

	inline static std::array const descriptors {
		ASDUData::Descriptor { Type::DOUBLEPOINT,			"double-point",	false,	Type::DOUBLEPOINT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::DOUBLEPOINT_WITH_TIMESTAMP,	"double-point",	true,	Type::DOUBLEPOINT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::SINGLEPOINT,			"single-point",	false,	Type::SINGLEPOINT,	SignalType::BOOLEAN },
		ASDUData::Descriptor { Type::SINGLEPOINT_WITH_TIMESTAMP,	"single-point",	true,	Type::SINGLEPOINT,	SignalType::BOOLEAN },
		ASDUData::Descriptor { Type::SCALED,				"scaled",	false,	Type::SCALED,		SignalType::INTEGER },
		ASDUData::Descriptor { Type::SCALED_WITH_TIMESTAMP,		"scaled",	true,	Type::SCALED,		SignalType::INTEGER },
		ASDUData::Descriptor { Type::NORMALIZED,			"normalized",	false,	Type::NORMALIZED,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::NORMALIZED_WITH_TIMESTAMP,		"normalized",	true,	Type::NORMALIZED,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::SHORT,				"short",	false,	Type::SHORT,		SignalType::FLOAT },
		ASDUData::Descriptor { Type::SHORT_WITH_TIMESTAMP,		"short",	true,	Type::SHORT,		SignalType::FLOAT },
	};

	ASDUData(ASDUData::Descriptor const &descriptor, int ioa);

	// descriptor within the descriptors table above
	ASDUData::Descriptor const &descriptor;
};

class SlaveNode : public Node {
protected:
	bool debug = true;

	struct Server {
		// slave state
		bool created = false;

		// config (use explicit defaults)
		std::string local_address = "0.0.0.0";
		int local_port = 2404;
		int low_priority_queue_size = 16;
		int high_priority_queue_size = 16;
		int common_address = 1;

		// config (use lib60870 defaults if std::nullopt)
		std::optional<int> apci_t0 = std::nullopt;
		std::optional<int> apci_t1 = std::nullopt;
		std::optional<int> apci_t2 = std::nullopt;
		std::optional<int> apci_t3 = std::nullopt;
		std::optional<int> apci_k = std::nullopt;
		std::optional<int> apci_w = std::nullopt;

		// lib60870
		CS104_Slave slave;
		CS101_AppLayerParameters asdu_app_layer_parameters;
	} server;

	struct Output {
		// config
		bool enabled = false;
		std::vector<ASDUData> mapping = {};
	} out;

	void createSlave() noexcept;
	void destroySlave() noexcept;

	void startSlave() noexcept(false);
	void stopSlave() noexcept;

	void debugPrintMessage(IMasterConnection connection, uint8_t* message, int message_size, bool sent) const noexcept;
	void debugPrintConnection(IMasterConnection connection, CS104_PeerConnectionEvent event) const noexcept;

	bool onClockSync(IMasterConnection connection, CS101_ASDU asdu, CP56Time2a new_time) const noexcept;
	bool onInterrogation(IMasterConnection connection, CS101_ASDU asdu, uint8_t _of_inter) const noexcept;
	bool onASDU(IMasterConnection connection, CS101_ASDU asdu) const noexcept;

	virtual
	int _write(struct Sample * smps[], unsigned cnt) override;

public:
	SlaveNode(const std::string &name = "");

	virtual
	~SlaveNode() override;

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;

	virtual
	int start() override;

	virtual
	int stop() override;

	// virtual
	// std::string & getDetails() override;
};

} /* namespace iec60870 */
} /* namespace node */
} /* namespace villas */
