/** Statistic collection.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#pragma once

#include <cstdint>
#include <jansson.h>

#include <unordered_map>
#include <memory>
#include <string>

#include <villas/common.h>
#include <villas/hist.hpp>
#include <villas/table.hpp>
#include <villas/signal.h>

/* Forward declarations */
struct sample;
struct node;

namespace villas {

class Stats {

public:
	enum class Format {
		HUMAN,
		JSON,
		MATLAB
	};

	enum class Metric {
		SMPS_SKIPPED,		/**< Counter for skipped samples due to hooks. */
		SMPS_REORDERED,		/**< Counter for reordered samples. */

		/* Timings */
		GAP_SAMPLE,		/**< Histogram for inter sample timestamps (as sent by remote). */
		GAP_RECEIVED,		/**< Histogram for inter sample arrival time (as seen by this instance). */
		OWD,			/**< Histogram for one-way-delay (OWD) of received samples. */
		AGE,			/**< Processing time of packets within VILLASnode. */

		/* RTP metrics */
		RTP_LOSS_FRACTION,	/**< Fraction lost since last RTP SR/RR. */
		RTP_PKTS_LOST,		/**< Cumul. no. pkts lost. */
		RTP_JITTER		/**< Interarrival jitter. */
	};

	enum class Type {
		LAST,
		HIGHEST,
		LOWEST,
		MEAN,
		VAR,
		STDDEV,
		TOTAL
	};

protected:
	std::unordered_map<Metric, villas::Hist> histograms;

	struct MetricDescription {
		const char *name;
		const char *unit;
		const char *desc;
	};

	struct TypeDescription {
		const char *name;
		enum SignalType signal_type;
	};

	static std::shared_ptr<Table> table;

	static void setupTable();

public:

	Stats(int buckets, int warmup);

	static
	enum Format lookupFormat(const std::string &str);

	static
	enum Metric lookupMetric(const std::string &str);

	static
	enum Type lookupType(const std::string &str);

	void update(enum Metric id, double val);

	void reset();

	json_t * toJson() const;

	static
	void printHeader(enum Format fmt);

	void printPeriodic(FILE *f, enum Format fmt, struct node *p) const;

	void print(FILE *f, enum Format fmt, int verbose) const;

	union signal_data getValue(enum Metric sm, enum Type st) const;

	const villas::Hist & getHistogram(enum Metric sm) const;

	static std::unordered_map<Metric, MetricDescription> metrics;
	static std::unordered_map<Type, TypeDescription> types;
	static std::vector<TableColumn> columns;
};

} /* namespace villas */
