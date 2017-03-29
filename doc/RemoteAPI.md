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

### `reload`

Restart VILLASnode with a new configuration file.

**Request:** _application/json_

	{
		"request": "reload",
		"id": "edei4shahNgucoo7paeth8chue0iyook"
		"configuration": "smb://MY-WINDOWS-HOST/SHARE1/path/to/config.conf"
	}

**Response:** _application/json_

	{
		"response": "reload",
		"id": "edei4shahNgucoo7paeth8chue0iyook",
		"status": "success"
	}

### `config`

Retrieve the contents of the current configuration.

**Request:** _application/json_

	{
		"request": "config",
		"id": "oom3lie7nee4Iepieng8ae4ueToebeki"
		"configuration": "smb://MY-WINDOWS-HOST/SHARE1/path/to/config.conf"
	}

**Response:** _application/json_

	{
		"response": "config",
		"id": "oom3lie7nee4Iepieng8ae4ueToebeki",
		"config" : {
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

_This request is not implemented yet_

### `paths`

Get a list of currently active paths.

_This request is not implemented yet_

### `status`

The the status of this VILLASnode instance.

_This request is not implemented yet_