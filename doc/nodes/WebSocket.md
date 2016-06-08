# WebSocket {#websocket}

The `websocket` node type spawns a HTTP / WebSocket server.

## Configuration

There is no node specific configuration.

### Example

	nodes = {
		ws = {
			type = "websocket"
		}
	}
	
	http = {
		port = 8080;
		htdocs = "/villas/contrib/websocket/";
		ssl_cert = "/etc/ssl/certs/mycert.pem";
		ssl_private_key= "/etc/ssl/private/mykey.pem";
	}