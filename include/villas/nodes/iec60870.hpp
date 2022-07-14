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

/// A supported CS101 information data type
class ASDUData {
public:
	enum Type {
		// SinglePointInformation
		SINGLE_POINT = M_SP_NA_1,
		// SinglePointWithCP56Time2a
		SINGLE_POINT_WITH_TIMESTAMP = M_SP_TB_1,
		// DoublePointInformation
		DOUBLE_POINT = M_DP_NA_1,
		// DoublePointWithCP56Time2a
		DOUBLE_POINT_WITH_TIMESTAMP = M_DP_TB_1,
		// MeasuredValueScaled
		SCALED_INT = M_ME_NB_1,
		// MeasuredValueScaledWithCP56Time2a
		SCALED_INT_WITH_TIMESTAMP = M_ME_TE_1,
		// MeasuredValueNormalized
		NORMALIZED_FLOAT = M_ME_NA_1,
		// MeasuredValueNormalizedWithCP56Time2a
		NORMALIZED_FLOAT_WITH_TIMESTAMP = M_ME_TD_1,
		// MeasuredValueShort
		SHORT_FLOAT = M_ME_NC_1,
		// MeasuredValueShortWithCP56Time2a
		SHORT_FLOAT_WITH_TIMESTAMP = M_ME_TF_1,
	};

	struct Sample {
		SignalData signal_data;
		QualityDescriptor quality;
		std::optional<timespec> timestamp;
	};

	// parse the config json
	static ASDUData parse(json_t *signal_json, std::optional<ASDUData> last_data, bool duplicate_ioa_is_sequence);

	// does this data include a timestamp
	bool hasTimestamp() const;
	// the IEC104 type
	ASDUData::Type type() const;
	// the config file identifier for this type
	char const * name() const;
	// get equivalent IEC104 type without timestamp (e.g. for general interrogation response)
	ASDUData::Type typeWithoutTimestamp() const;
	// get equivalent ASDUData without timestamp (e.g. for general interrogation response)
	ASDUData withoutTimestamp() const;
	// corresponding signal type
	SignalType signalType() const;
	// check if ASDU contains this data
	std::optional<ASDUData::Sample> checkASDU(CS101_ASDU const &asdu) const;
	// add SignalData to an ASDU, returns false when sample couldn't be added (insufficient space in ASDU)
	bool addSampleToASDU(CS101_ASDU &asdu, ASDUData::Sample sample) const;

	// every value in an ASDU has an associated "information object address" (ioa)
	int ioa;
	// start of the ioa sequence
	int ioa_sequence_start;
private:
	struct Descriptor {
		ASDUData::Type type;
		char const *name;
		char const *type_id;
		bool has_timestamp;
		ASDUData::Type type_without_timestamp;
		SignalType signal_type;
	};

	inline static std::array const descriptors {
		ASDUData::Descriptor { Type::SINGLE_POINT,			"single-point",		"M_SP_NA_1",	false,	Type::SINGLE_POINT,	SignalType::BOOLEAN },
		ASDUData::Descriptor { Type::SINGLE_POINT_WITH_TIMESTAMP,	"single-point",		"M_SP_TB_1",	true,	Type::SINGLE_POINT,	SignalType::BOOLEAN },
		ASDUData::Descriptor { Type::DOUBLE_POINT,			"double-point",		"M_DP_NA_1",	false,	Type::DOUBLE_POINT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::DOUBLE_POINT_WITH_TIMESTAMP,	"double-point",		"M_DP_TB_1",	true,	Type::DOUBLE_POINT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::SCALED_INT,			"scaled-int",		"M_ME_NB_1",	false,	Type::SCALED_INT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::SCALED_INT_WITH_TIMESTAMP,		"scaled-int",		"M_ME_TB_1",	true,	Type::SCALED_INT,	SignalType::INTEGER },
		ASDUData::Descriptor { Type::NORMALIZED_FLOAT,			"normalized-float",	"M_ME_NA_1",	false,	Type::NORMALIZED_FLOAT,	SignalType::FLOAT },
		ASDUData::Descriptor { Type::NORMALIZED_FLOAT_WITH_TIMESTAMP,	"normalized-float",	"M_ME_TA_1",	true,	Type::NORMALIZED_FLOAT,	SignalType::FLOAT },
		ASDUData::Descriptor { Type::SHORT_FLOAT,			"short-float",		"M_ME_NC_1",	false,	Type::SHORT_FLOAT,	SignalType::FLOAT },
		ASDUData::Descriptor { Type::SHORT_FLOAT_WITH_TIMESTAMP,	"short-float",		"M_ME_TC_1",	true,	Type::SHORT_FLOAT,	SignalType::FLOAT },
	};

	ASDUData(ASDUData::Descriptor const *descriptor, int ioa, int ioa_sequence_start);

	// lookup datatype for config key asdu_type
	static std::optional<ASDUData> lookupName(char const *name, bool with_timestamp, int ioa, int ioa_sequence_start);
	// lookup datatype for config key asdu_type_id
	static std::optional<ASDUData> lookupTypeId(char const *type_id, int ioa, int ioa_sequence_start);
	// lookup datatype for numeric type identifier
	static std::optional<ASDUData> lookupType(int type, int ioa, int ioa_sequence_start);

	// descriptor within the descriptors table above
	ASDUData::Descriptor const *descriptor;
};

class SlaveNode : public Node {
protected:
	bool debug = true;

	struct Server {
		// slave state
		enum { NONE, STOPPED, READY } state = NONE;

		// config (use explicit defaults)
		std::string local_address = "0.0.0.0";
		int local_port = 2404;
		int common_address = 1;
		int low_priority_queue = 100;
		int high_priority_queue = 100;

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
		std::vector<ASDUData::Type> asdu_types = {};

		mutable std::mutex last_values_mutex;
		std::vector<SignalData> last_values = {};
	} output;

	void createSlave() noexcept;
	void destroySlave() noexcept;

	void startSlave() noexcept(false);
	void stopSlave() noexcept;

	void debugPrintMessage(IMasterConnection connection, uint8_t *message, int message_size, bool sent) const noexcept;
	void debugPrintConnection(IMasterConnection connection, CS104_PeerConnectionEvent event) const noexcept;

	bool onClockSync(IMasterConnection connection, CS101_ASDU asdu, CP56Time2a new_time) const noexcept;
	bool onInterrogation(IMasterConnection connection, CS101_ASDU asdu, uint8_t _of_inter) const noexcept;
	bool onASDU(IMasterConnection connection, CS101_ASDU asdu) const noexcept;

	void sendPeriodicASDUsForSample(Sample const *sample) const noexcept(false);

	virtual
	int _write(struct Sample *smps[], unsigned cnt) override;

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
