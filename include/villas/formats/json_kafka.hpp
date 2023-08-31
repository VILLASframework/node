/* JSON serializtion for Kafka schema/payloads.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

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

} // namespace node
} // namespace villas
