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
class ASDUDataType {
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

	// check if ASDU type is supported
	static std::optional<ASDUDataType> checkASDU(CS101_ASDU const &asdu);
	// infer appropriate DataType for SignalType
	static std::optional<ASDUDataType> inferForSignal(SignalType type);

	// does this data include a timestamp
	bool hasTimestamp() const;

	// get equivalent DataType without timestamp (e.g. for general interrogation response)
	ASDUDataType withoutTimestamp() const;

	// is DataType convertible to/from SignalType
	bool isConvertibleFromSignal(SignalType signal_type) const;

	// add SignalData to an ASDU
	void addSignalsToASDU(
		CS101_ASDU &asdu,
		int ioa,
		QualityDescriptor quality,
		SignalType signal_type,
		SignalData *signal_data,
		unsigned signal_count,
		std::optional<timespec> timestamp
	) const;

	// basic conversions and comparisons
	ASDUDataType(Type type);
	operator Type() const;
	bool operator==(ASDUDataType data_type) const;
	bool operator!=(ASDUDataType data_type) const;
private:
	Type type;
};

class TcpNode : public Node {
protected:
	struct Server {
		bool created = false;

		std::string local_address = "0.0.0.0";
		int local_port = 2404;
		int low_priority_queue_size = 16;
		int high_priority_queue_size = 16;

		CS104_Slave slave;
	} server;

	struct Output {
		bool enabled = false;
		SignalType signal_type = SignalType::INVALID;
		ASDUDataType asdu_data_type = ASDUDataType::SHORT_WITH_TIMESTAMP;
		unsigned signal_cnt = 0;
	} out;

	virtual
	int _read(struct Sample * smps[], unsigned cnt) override;

	virtual
	int _write(struct Sample * smps[], unsigned cnt) override;

public:
	TcpNode(const std::string &name = "");

	virtual
	~TcpNode() override;
	/*
	virtual
	int start() override;

	virtual
	int stop() override;

	virtual
	std::string & getDetails() override;

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;
	*/
};

} /* namespace iec60870 */
} /* namespace node */
} /* namespace villas */
