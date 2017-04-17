#Remote Application Programming Interface (API) {#API}

VILLASnode can be controlled remotely with an Application Programming Interface (API).

## Transports

The API is accessible via multiple transports:

- HTTP POST requests
- WebSockets
- Unix Domain sockets (planned)

### HTTP REST

**Endpoint URL:** `http[s]://server:port/api/v1`

**HTTP Method:** POST only

### WebSockets

**Protocol:** `api`

**Endpoint:** `ws[s]://server:port/v1`

### Unix socket

_This transport is not implemented yet_

## Protocol

All transports use the same JSON-based protocol to encode API requests and responses.
Both requests and responses are JSON objects with the fields described below.
Per API session multiple requests can be sent to the node.
The order of the respective responses does not necessarily match the order of the requests.
Requests and responses can be interleaved.
The client must check the `id` field in order to find the matching response to a request.

#### Request

| Field		| Description	|
|:------------------------ |:---------------------- |
| `action`	| The API action which is requested. See next section. |
| `id`		| A unique string which identifies the request. The same id will be used for the response. | 
| `request`	| Any JSON data-type which will be passed as an argument to the respective API action. |

#### Response

The response is similar to the request object.
The `id` field of the request will be copied to allow

| Field		| Description	|
|:------------------------ |:---------------------- |
| `action`	| The API action which is requested. See next section. |
| `id`		| A unique string which identifies the request. The same id will be used for the response. | 
| `response`	| Any JSON data-type which can be returned. |

## Actions

### `restart`

Restart VILLASnode with a new configuration file.

**Request:** _application/json_

	{
		"action": "restart",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
		"request" : {
			"configuration": "smb://MY-WINDOWS-HOST/SHARE1/path/to/config.conf"
		}
	}

**Response:** _application/json_

	{
		"action": "reload",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
		"response" : "success"
	}

### `config`

Retrieve the contents of the current configuration.

**Request:** _application/json_

	{
		"action": "config",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
	}

**Response:** _application/json_

	{
		"action": "config",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a",
		"response" : {
			"nodes" : {
				"socket_node" : {
					"type": "socket",
					"layer": "udp",
					...
				} 
			},
			"paths" : [
				{
					"in": "socket_node",
					"out": "socket_node"
				}
			]
		}
	}

### `nodes`

Get a list of currently active nodes.

**Request:** _application/json_

	{
		"action": "nodes",
		"id": "5a786626-fbc6-4c04-98c2-48027e68c2fa"
	}

**Response:** _application/json_

	{
		"action": "nodes",
		"response": [
			{
				"name": "ws",
				"state": 4,
				"vectorize": 1,
				"affinity": 0,
				"id": 0,
				"type": "websocket",
				"description": "Demo Channel"
			}
		],
		"id": "5a786626-fbc6-4c04-98c2-48027e68c2fa"
	}

### `capabilities`

Get a list of supported node-types, hooks and API actions.

### `paths`

Get a list of currently active paths.

_This request is not implemented yet_

### `status`

The the status of this VILLASnode instance.

_This request is not implemented yet_