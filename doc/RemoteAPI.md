#Remote Application Programming Interface (API) {#API}

VILLASnode can be controlled remotely over a HTTP REST / WebSocket API.
This page documents this API.

## Transports

The API is accessible via multiple transports:

- via HTTP POST requests
- via a WebSocket protocol
- via a Unix socket

All transports use the same JSON based protocol to handle requests.

### HTTP REST

**Endpoint URL:** http[s]://localhost:80/api/v1
**HTTP Method:** POST

### WebSockets

**WebSocket Protocol:** `api`

### Unix socket

_This transport is not implemented yet_

## Commands

### `restart`

Restart VILLASnode with a new configuration file.

**Request:** _application/json_

	{
		"request": "restart",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
		"args" : {
			"configuration": "smb://MY-WINDOWS-HOST/SHARE1/path/to/config.conf"
		}
	}

**Response:** _application/json_

	{
		"response": "reload",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
	}

### `config`

Retrieve the contents of the current configuration.

**Request:** _application/json_

	{
		"request": "config",
		"id": "66675eb4-6a0b-49e6-8e82-64d2b2504e7a"
	}

**Response:** _application/json_

	{
		"response": "config",
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
		"request": "nodes",
		"id": "5a786626-fbc6-4c04-98c2-48027e68c2fa"
	}

**Response:** _application/json_

	{
		"command": "nodes",
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

### `paths`

Get a list of currently active paths.

_This request is not implemented yet_

### `status`

The the status of this VILLASnode instance.

_This request is not implemented yet_