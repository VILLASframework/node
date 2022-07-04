/** JSON serializtion for Kafka schema/payloads.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/signal_type.hpp>
#include <villas/formats/json.hpp>

namespace villas {
namespace node {

class JsonKafkaFormat : public JsonFormat {

protected:
	virtual
	int packSample(json_t **j, const struct Sample *smp);
	virtual
	int unpackSample(json_t *json_smp, struct Sample *smp);

	const char * villasToKafkaType(enum SignalType vt);

	json_t *json_schema;

public:
	JsonKafkaFormat(int fl);

	virtual
	void parse(json_t *json);
};

} /* namespace node */
} /* namespace villas */
