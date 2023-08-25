/** Node type: Client for the Universal Data-exchange API (v2)
 *
 * @see https://github.com/ERIGrid2/JRA-3.1-api
 * @file
 * @author Andres Acosta <andres.acosta@eonerc.rwth-aachen.de>
 * @copyright 2014-2023, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <pthread.h>
#include <curl/curl.h>
#include <jansson.h>

#include <villas/node.hpp>
#include <villas/nodes/api.hpp>
#include <villas/api/universal.hpp>
#include <villas/list.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class APIClientNode : public APINode {

public:
	APIClientNode(const uuid_t &id = {}, const std::string &name = "");

	// #ifdef CURL_SSL_REQUIRES_LOCKING
	// pthread_mutex_t *mutex_buf;
	// #endif /* CURL_SSL_REQUIRES_LOCKING */

	struct ClientDirection : Direction {
		CURL *curl;
	};

	ClientDirection read, write;

	virtual
	~APIClientNode();

	// virtual
	// int prepare();

	/** Validate node configuration. */
	// virtual
	// int check();

	virtual
	int start();

	virtual
	int stop();

	virtual
	int reverse();

	// virtual
	// std::vector<int> getNetemFDs();

	// virtual
	// struct villas::node::memory::Type * getMemoryType();

	// virtual
	// const std::string & getDetails();

protected:
	virtual
	int parse(json_t *json);

	const char *endpoint;
	const char *access_token;

	double timeout;
	double rate;

	struct Task task;
	int ssl_verify;

	struct curl_slist *headers;

	int parse_channel_response(json_t *json_response, int *code, char **reason, json_t **json_rvalue, Logger logger);
	// int request(CURL *handle, const char *endpoint, const char *operation, json_t *json_request, json_t **json_response, Logger logger);
	int request_channel_query(CURL *handle, const char *endpoint, const char *signal_name, json_t **json_rvalue, Logger logger);
	// int request_channel_update(CURL *handle, const char *endpoint, const char *action, json_t *json_entity, Logger logger);

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);
};

// struct api_client {
// 	const char *endpoint;		/**< The REST API client context broker endpoint URL. */
// 	const char *access_token;	/**< An optional authentication token which will be sent as HTTP header. */

// 	double timeout;			/**< HTTP timeout in seconds */
// 	double rate;			/**< Rate used for polling. */

// 	struct Task task;		/**< Timer for periodic events. */
// 	int ssl_verify;			/**< Boolean flag whether SSL server certificates should be verified or not. */

// 	struct curl_slist *headers;	/**< List of HTTP request headers for libcurl */

// 	struct {
// 		CURL *curl;		/**< libcurl: handle */
// 		struct List signals;	/**< A mapping between indices of the VILLASnode samples and the attributes in api_client::context */
// 	} in, out;
// };


// int api_client_type_start(SuperNode *sn);

// int api_client_type_stop();

// char * api_client_print(NodeCompat *n);

// int api_client_init(NodeCompat *n);

// int api_client_reverse(NodeCompat *n);

// int api_client_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
