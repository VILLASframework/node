function Api(v, connected, error)
{
	this.version = v
	this.ids = new Object();
	
	var url = wsUrl('v1');
	var self = this;

	this.connection = new WebSocket(url, 'api');

	this.connection.onopen = function() {
		console.log('API connected', this.url);
		connected();
	};

	this.connection.onerror = function(e) {
		console.log('API request failed:', e);
		error(e);
	};

	this.connection.onmessage = function(e) {
		var resp = JSON.parse(e.data);
		var handler;
		
		console.log('API response received', resp);
		
		handler = self.ids[resp.id];
		if (handler !== undefined) {
			handler(resp.response);
			
			delete self.ids[resp.id];
		}
	};
}

Api.prototype.request = function(action, request, handler)
{
	var req = {
		action : action,
		request: request,
		id : guid()
	};
	
	this.ids[req.id] = handler;
	
	console.log('API request sent', req);

	this.connection.send(JSON.stringify(req))
}

Api.prototype.close = function()
{
	this.connection.close();
}