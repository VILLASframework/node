// global variables
var connection;

var seq = 0;

var node = getParameterByName("node") || "ws";
var url = wsUrl(node);
var protocol = ['live'];
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

var msg = new Msg();

var xPast  = 10*1000;
var xFuture = 5*1000;

$(document).on('ready', function() {
	$.getJSON('/nodes.json', function(data) {
		$(data).each(function(index, n) {
			var title = n.description ? n.description : n.name;
			
			if (n.unit)
				title += " [" + n.unit + "]";
			
			$(".node-selector").append(
				$("<li>").append(
					$("<a>", {
						text: title,
						title: n.name,
						href: "?node=" + n.name
					})
				).addClass(n.name == node ? 'active' : '')
			);
		});
	});
	
	$('#slider').slider({
		min : 0,
		max : 100,
		slide : onSliderMoved
	});
	
	$('#controls .buttons button').each(function(button) {
		$(button).addClass('on');
	});
	
	wsConnect();
	
	setInterval(plotUpdate, updateRate);
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

function wsConnect() {
	connection = new WebSocket(url, protocol);
	
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
		var msg = new Msg();
		
		msg.parse(e.data);

		// add empty arrays for data
		while (plotData.length < msg.values.length)
			plotData.push([]);

		// add data to arrays
		for (var i = 0; i < msg.values.length; i++)
			plotData[i].push([msg.ts, msg.values[i]]);
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
