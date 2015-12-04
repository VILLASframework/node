// global variables
var connection;
var timer;

var seq = 0;

var node = 'websocket'
var url = 'ws://10.211.55.6:8080/' + node;
var protocol = ['live'];

var plotData = [];
var plotOptions = {
	xaxis: {
		mode: 'time'
	},
	legend: {
		show: true
	}
};

var xDelta = 10*1000;

$(document).on('ready', function() {
	$.getJSON('/nodes.json', function(data) {
		$(data).each(function(index, node) {
			$(".node-selector").append($("<li>").text(node));
		});
	});
	
	$('#slider').slider({
		min : 0,
		max : 100,
		slide : function(event, ui) {
			sendMsg(Date.now(), [ ui.value ]);
		}
	});
	
	wsConnect();
});

$(window).on('beforeunload', wsDisconnect);

function plotUpdate() {
	var data = [];

	// add data to arrays
	for (var i = 0; i < plotData.length; i++) {
		// remove old values
		//while (plotData[i].length > 0 && plotData[i][0][0] < (Date.now() - xDelta))
		//	plotData[i].shift()
		
		data[i] = {
			data : plotData[i],
			shadowSize : 0,
			label : "Index " + String(i),
			lines : {
				lineWidth: 2
			}
		}
	}
	
	var options = {
		xaxis: {
			min: Date.now() - xDelta,
			max: Date.now()
		}
	}

	/* update plot */
	$.plot('#placeholder', data, $.extend(true, options, plotOptions));
}

function wsDisconnect() {
	connection.close();
	
	$('#connectionStatus')
		.text('Disconnected')
		.css('color', 'red');

	plotData = [];
	clearInterval(timer);
}

function wsConnect() {
	connection = new WebSocket(url, protocol);
	
	connection.onopen = function() {
		$('#connectionStatus')
			.text('Connected')
			.css('color', 'green');
		
		timer = setInterval(plotUpdate, 1.0 / 25);
	};

	connection.onclose = function() {
		wsDisconnect();
		
		setTimeout(wsConnect, 3000); // retry
	};

	connection.onerror = function(error) {
		$('#connectionStatus').text(function() {
			return 'Status: Error: ' + error.message;
		});
	};

	connection.onmessage = function(e) {
		console.log(e.data);
		
		var res = e.data.split(/\s+/).map(Number);
		var timestamp = res[0] * 1000;
		var values = res.slice(1)

		// add empty arrays for data
		while (plotData.length < values.length)
			plotData.push([]);

		// add data to arrays
		for (var i = 0; i < values.length; i++)
			plotData[i].push([timestamp, values[i]]);
	};	
};

function sendMsg(ts, values) {
	connection.send(ts / 1000 + "(" + seq + ") " + values.join(" "));
	
	seq += 1;
}

/* Control event handlers */
function onButtonClick(value) {
	sendMsg(Date.now(), [ value ]);
}

function onTextChange() {
	sendMsg(Date.now(), [ 1 ]);
}

/* Helpers */
function wsUrl(endpoint) {
	var l = window.location;
	
	var protocol = (l.protocol === "https:") ? "wss://" : "ws://";
	var port     = ((l.port != 80) && (l.port != 443)) ? ":" + l.port : "";

	return protocol + l.hostname + port + endpoint;
}


