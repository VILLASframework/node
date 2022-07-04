/** Node type: kafka
 *
 * @file
 * @author Juan Pablo Noreña <jpnorenam@unal.edu.co>
 * @copyright 2021, Universidad Nacional de Colombia
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <librdkafka/rdkafka.h>

#include <villas/pool.hpp>
#include <villas/format.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct kafka {
	struct CQueueSignalled queue;
	struct Pool pool;

	double timeout;			/**< Timeout in seconds. */
	char *server;			/**< Hostname/IP:Port address of the bootstrap server. */
	char *protocol;			/**< Security protocol. */
	char *produce;			/**< Producer topic. */
	char *consume;			/**< Consumer topic. */
	char *client_id;		/**< Client ID. */

	struct {
		rd_kafka_t *client;
		rd_kafka_topic_t *topic;
	} producer;

	struct {
		rd_kafka_t *client;
		char *group_id;		/**< Group id. */
	} consumer;

	struct {
		char *ca;	/**< SSL CA file. */
	} ssl;

	struct {
		char *mechanisms;	/**< SASL mechanisms. */
		char *username;		/**< SSL CA path. */
		char *password;		/**< SSL certificate. */
	} sasl;

	Format *formatter;
};

int kafka_reverse(NodeCompat *n);

char * kafka_print(NodeCompat *n);

int kafka_init(NodeCompat *n);

int kafka_prepare(NodeCompat *n);

int kafka_parse(NodeCompat *n, json_t *json);

int kafka_start(NodeCompat *n);

int kafka_destroy(NodeCompat *n);

int kafka_stop(NodeCompat *n);

int kafka_type_start(SuperNode *sn);

int kafka_type_stop();

int kafka_poll_fds(NodeCompat *n, int fds[]);

int kafka_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int kafka_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
