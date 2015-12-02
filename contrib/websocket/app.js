// global variables
var connection;
var timer;

var url = 'ws://134.130.169.31/test';
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

var xDelta = 3*1000;

$(document).on('ready', function() {
	$('#slider').slider({
		min : 0,
		max : 100,
		slide : function(event, ui) {
			sendMsg(Date.now(), [ ui.value ]);
		}
	});
	
	connect();
});

$(window).on('beforeunload', disconnect);

// update plot
function update() {
	var data = [];

	// add data to arrays
	for (var i = 0; i < plotData.length; i++) {
		// remove old values
		while (plotData[i][0][0] < (Date.now() - xDelta))
			plotData[i].shift()
		
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

	// update plot
	$.plot('#placeholder', data, $.extend(true, options, plotOptions));
}

function disconnect() {
	connection.close();
	
	$('#connectionStatus')
		.text('Disconnected')
		.css('color', 'red');

	plotData = [];
	clearInterval(timer);
}

function connect() {
	connection = new WebSocket(url, protocol);
	
	connection.onopen = function() {
		$('#connectionStatus')
			.text('Connected')
			.css('color', 'green');
		
		timer = setInterval(update, 1.0 / 25);
	};

	connection.onclose = function() {
		disconnect();
		
		setTimeout(connect, 3000); // retry
	};

	connection.onerror = function(error) {
		$('#connectionStatus').text(function() {
			return 'Status: Error: ' + error.message;
		});
	};

	connection.onmessage = function(e) {
		var res = e.data.split(/\s+/).map(Number);
		var timestamp = res[0] * 1000;
		var values = res.slice(1)
		var data = [];

		// add empty arrays for data
		while (plotData.length < values.length)
			plotData.push([]);

		// add data to arrays
		for (var i = 0; i < values.length; i++)
			plotData[i].push([timestamp, values[i]]);
	};	
};

function sendMsg(ts, values) {
	connection.send(ts / 1000 + " " + values.join(" "));
}

function onButtonClick(value) {
	sendMsg(Date.now, [ value ])
};

