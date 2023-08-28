/** Node-type for stats streaming.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <jansson.h>

#include <villas/stats.hpp>
#include <villas/task.hpp>
#include <villas/list.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct stats_node_signal {
	Node *node;
	char *node_str;

	enum villas::Stats::Metric metric;
	enum villas::Stats::Type type;
};

struct stats_node {
	double rate;

	struct Task task;

	struct List signals; /** List of type struct stats_node_signal */
};

int stats_node_type_start(SuperNode *sn);

int stats_node_init(NodeCompat *n);

int stats_node_destroy(NodeCompat *n);

char *stats_node_print(NodeCompat *n);

int stats_node_parse(NodeCompat *n, json_t *json);

int stats_node_prepare(NodeCompat *n);

int stats_node_start(NodeCompat *n);

int stats_node_stop(NodeCompat *n);

int stats_node_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int stats_node_parse_signal(struct stats_node_signal *s, json_t *json);

int stats_node_signal_destroy(struct stats_node_signal *s);

int stats_node_signal_parse(struct stats_node_signal *s, json_t *json);

int stats_node_poll_fds(NodeCompat *n, int fds[]);

} // namespace node
} // namespace villas
