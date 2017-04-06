// global variables
var connection;
var timer;

var seq = 0;

var currentNode;
var nodes = [ ];

var plotData = [];
var plotOptions = {
	xaxis: {
		mode: 'time'
	},
	legend: {
		show: true
	}
};

var xDelta   = 0.5*1000;
var xPast   = xDelta*0.9;
var xFuture = xDelta*0.1;

$(document).on('ready', function() {
	$.getJSON('/nodes.json', function(data) {
		nodes = data;
		
		for (var i = 0; i < nodes.length; i++)
			if (nodes[i].name == getParameterByName("node"))
				currentNode = nodes[i];
		
		if (currentNode === undefined)
			currentNode = nodes[0];
		
		nodes.forEach(function(node, index) {
			$(".node-selector").append(
				$("<li>").append(
					$("<a>", {
						text: node.description ? node.description : node.name,
						title: node.name,
						href: "?node=" + node.name
					})
				).addClass(node.name == currentNode.name ? 'active' : '')
			);
		});
		
		wsConnect(wsUrl(currentNode.name), ["live"]);
	});
	
	$('#play').click(function(e, ui) {
		wsConnect(wsUrl(currentNode.name), ["live"]);
	});
	
	$('#pause').click(function(e, ui) {
		connection.close();
	});

	$('#slider').slider({
		min : 0,
		max : 100,
		slide : function(e, ui) {
			var msg = new Msg({
				timestamp : Date.now(),
				sequence  : seq++
			}, [ ui.value ]);
	
	
			var blob = msg.toArrayBuffer()
			connection.send(blob);
		}
	});
	
	$('#timespan').slider({
		min : 200,
		max : 10000,
		value : xDelta,
		slide : function(e, ui) {
			plotUpdateWindow(ui.value);
		}
	});
	
	$('#controls .buttons button').each(function(button) {
		$(button).addClass('on');
		
		$(button).onClick(function(value) {
			var msg = new Msg({
				timestamp : Date.now(),
				sequence  : seq++
			}, [ value ]);
	
			connection.send(msg.toArrayBuffer());
		});
	});
	
	plotUpdateWindow(10*1000); /* start plot refresh timer for 10sec window */
});

$(window).on('beforeunload', function() {
	connection.close();
});

function plotUpdateWindow(delta) {
	xDelta  = delta
	xPast   = xDelta*0.9;
	xFuture = xDelta*0.1;
}

function plotUpdate() {
	var data = [];

	// add data to arrays
	for (var i = 0; i < plotData.length; i++) {
		var seriesOptions = nodes
		
		data[i] = {
			data : plotData[i],
			shadowSize : 0,
			label : "Index " + String(i),
			lines : {
				lineWidth: 2
			}
		}
		
		if (currentNode.series !== undefined && currentNode.series[i] !== undefined)
			$.extend(true, data[i], currentNode.series[i]);
	}
	
	var options = {
		xaxis: {
			min: Date.now() - xPast,
			max: Date.now() + xFuture
		},
		grid: {
			markings: [
				{ xaxis: { from: Date.now(), to: Date.now() }, color: "#ff0000" }
			]
		}
	}

	/* update plot */
	$.plot('.plot-container div', data, $.extend(true, options, plotOptions));
}

function wsConnect(url, protocol) {
	connection = new WebSocket(url, protocol);
	connection.binaryType = 'arraybuffer';
	
	connection.onopen = function() {
		$('#connectionStatus')
			.text('Connected')
			.css('color', 'green');
			
		timer = setInterval(plotUpdate, 1000.0 / 25);
	};

	connection.onclose = function() {
		$('#connectionStatus')
			.text('Disconnected')
			.css('color', 'red');

		clearInterval(timer);
		
		setTimeout(function() {
			wsConnect(wsUrl(currentNode.name), ["live"]);
		}, 1000); // retry
	};

	connection.onerror = function(error) {
		$('#connectionStatus').text(function() {
			return 'Status: Error: ' + error.message;
		});
	};

	connection.onmessage = function(e) {
		var msgs = Msg.fromArrayBufferVector(e.data);
		
		console.log('Received ' + msgs.length + ' messages with ' + msgs[0].data.length + ' values from id ' + msgs[0].id + ' with timestamp ' + msgs[0].timestamp);

		for (var j = 0; j < plotData.length; j++) {
			// remove old
			while (plotData[j].length > 0 && plotData[j][0][0] < (Date.now() - xPast))
				plotData[j].shift()
		}

		for (var j = 0; j < msgs.length; j++) {
			var msg = msgs[j];

			// add empty arrays for data series
			while (plotData.length < msg.length)
				plotData.push([]);

			// add data to arrays
			for (var i = 0; i < msg.length; i++)
				plotData[i].push([msg.timestamp, msg.data[i]]);
		}
	};	
};

/* Helpers */
function wsUrl(endpoint) {
	var l = window.location;
	var url = "";

	if (l.protocol === "https:")
		url += "wss://";
	else
		url += "ws://";
	
	url += l.hostname;
	
	if ((l.port) && (l.port != 80) && (l.port != 443))
		url += ":" + l.port;

	url += "/" + endpoint;
	
	return url;
}

/* Some helpers */
