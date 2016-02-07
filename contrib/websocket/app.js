// global variables
var connection;

var seq = 0;

var currentNode;
var nodes = [ ];
var updateRate = 1.0 / 25;

var plotData = [];
var plotOptions = {
	xaxis: {
		mode: 'time'
	},
	legend: {
		show: true
	}
};

var xPast  = 10*1000;
var xFuture = 5*1000;

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
	
	$('#slider').slider({
		min : 0,
		max : 100,
		slide : onSliderMoved
	});
	
	$('#controls .buttons button').each(function(button) {
		$(button).addClass('on');
	});
	
	setInterval(plotUpdate, updateRate);
});

$(window).on('beforeunload', wsDisconnect);

function plotUpdate() {
	var data = [];

	// add data to arrays
	for (var i = 0; i < plotData.length; i++) {
		// remove old values
		while (plotData[i].length > 0 && plotData[i][0][0] < (Date.now() - xPast))
			plotData[i].shift()
			
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

function wsDisconnect() {
	connection.close();
	
	$('#connectionStatus')
		.text('Disconnected')
		.css('color', 'red');

	plotData = [];
}

function wsConnect(url, protocol) {
	connection = new WebSocket(url, protocol);
	connection.binaryType = 'arraybuffer';
	
	
	connection.onopen = function() {
		$('#connectionStatus')
			.text('Connected')
			.css('color', 'green');
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
		var msgs = Msg.fromArrayBufferVector(e.data);

		for (var j = 0; j < msgs.length; j++) {
			var msg = msgs[j];

			// add empty arrays for data
			while (plotData.length < msg.values)
				plotData.push([]);

			// add data to arrays
			for (var i = 0; i < msg.values; i++)
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

function fileStart(e) {
	var file = e.target.files[0];
	var reader = new FileReader();
	
	var start;
	var msgs = [ ]
	var position = 0;

	reader.onload = function(e) {
		var lines = e.target.result.split(/[\r\n]+/g); // tolerate both Windows and Unix linebreaks

		for (var i = 0; i < lines.length; i++) {
			var msg = new Msg();
			msg.parse(lines[i]);
			msgs.push(msg);
		}

		console.log("Read " + msgs.length + " samples from file");

		if (msgs.length > 0) {
			var offset = Date.now() - msgs[0].ts;
			var data = [];
			
			for (var i = 0; i < msgs.length; i++)
				data.push(msgs[i].ts + offset, msgs[i].values[0]);
		
			plotData.push(data);
		}
		else {
			
		}
	};

	reader.readAsText(file);
}

/* Control event handlers */
function onButtonClick(value) {
	msg.values = [ value ];
	msg.ts = Date.now();
	msg.seq++;
	
	msg.send(connection);
}

function onTextChange(e) {
	msg.values = [ e.target.text ];
	msg.ts = Date.now();
	msg.seq++;
	
	msg.send(connection);
}

function onSliderMoved(e, ui) {
	msg.values = [ ui.value  ];
	msg.ts = Date.now();
	msg.seq++;
	
	msg.send(connection);
}

/* Some helpers */
function getParameterByName(name) {
	var match = RegExp('[?&]' + name + '=([^&]*)').exec(window.location.search);
	return match && decodeURIComponent(match[1].replace(/\+/g, ' '));
}
