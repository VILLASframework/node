/** VILLASnode WebMockup Javascript application.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

// global variables
var api;
var connection;
var timer;
var plot;

var nodes = [];
var currentNode;

var paused = false;
var sequence = 0;

var plotData = [];
var plotOptions = {
	xaxis: {
		mode: 'time'
	},
	legend: {
		show: true
	},
	selection: {
		mode: "x"
	}
};

var updateRate = 25;
var redrawPlot = true;

var xDelta  = 5000;
var xPast   = xDelta * 0.9;
var xFuture = xDelta * 0.1;

$(document).ready(function() {
	api = new Api('v1', apiConnected);

	$('#play').click(function(e, ui) {
		connection = wsConnect(currentNode);
		paused = false;
	});

	$('#pause').click(function(e, ui) {
		connection.close();
		paused = true;
	});

	$('#timespan').slider({
		min : 100,
		max : 10000,
		value : xDelta,
		slide : function(e, ui) {
			updatePlotWindow(ui.value);
		}
	});

	$('#updaterate').slider({
		min : 1,
		max : 100,
		value : updateRate,
		slide : function(e, ui) {
			clearInterval(timer);
			timer = setInterval(updatePlot, 1000.0 / updateRate);
			updateRate = ui.value;
		}
	});

	$('.inputs #slider').slider({
		min : 0,
		max : 10000,
		slide : sendData
	});

	$('.inputs-checkboxes input').checkboxradio()
		.each(function(idx, elm) {
			$(elm).change(sendData);
		});

	timer = setInterval(updatePlot, 1000.0 / updateRate);
});

$(window).on('beforeunload', function() {
	connection.close();
	api.close();
});

function sendData()
{
	var slider = $('.inputs #slider');
	var checkboxes = $('.inputs-checkboxes  input');

	var data = [ $(slider).slider('value') / 100, 0 ];

	for (var i = 0; i < checkboxes.length; i++)
		data[1] += (checkboxes[i].checked ? 1 : 0) << i;

	var msg = new Msg({
		timestamp : Date.now(),
		sequence  : sequence++,
		data      : data
	});

	console.log('Sending message', msg);

	connection.send(msg.toArrayBuffer());
}

function apiConnected()
{
	api.request('nodes', {},
		function(response) {
			nodes = response;

			console.log("Found " + nodes.length + " nodes:",);

			nodes.forEach(function(node) {
				console.log(node);
				if (node.type == 'websocket' && node.name == getParameterByName('node'))
					currentNode = node;
			});

			if (currentNode === undefined) {
				nodes.forEach(function(node) {
					if (node.type == 'websocket')
						currentNode = node;
				});
			}

			if (currentNode !== undefined) {
				updateNodeList();
				connection = wsConnect(currentNode);
			}
		}
	);
}

function updateNodeList()
{
	$('.node-selector').empty();

	nodes.forEach(function(node, index) {
		if (node.type == 'websocket') {
			$('.node-selector').append(
				$('<button>')
					.addClass(node.name == currentNode.name ? 'ui-state-active' : '')
					.text(node.description ? node.description : node.name)
					.click(function() {
						var url = node.name;
						window.location = '?node=' + node.name;
					})
			);
		}
	});

	$('.node-selector').buttonset();
}

function updatePlotWindow(delta)
{
	xDelta  = delta
	xPast   = xDelta * 0.9;
	xFuture = xDelta * 0.1;
}

function updatePlot()
{
	var data = [];

	if (!redrawPlot)
		return;

	// add data to arrays
	for (var i = 0; i < plotData.length; i++) {
		var seriesOptions = nodes;

		data[i] = {
			data : plotData[i],
			shadowSize : 0,
			label : 'Index ' + String(i),
			lines : {
				lineWidth: 2
			}
		}

		if (currentNode.out.signals !== undefined && currentNode.out.signals[i] !== undefined)
			$.extend(true, data[i], {
				label : currentNode.out.signals[i].name
			});
	}

	var options = {
		xaxis: {
			min: Date.now() - xPast,
			max: Date.now() + xFuture
		},
		grid: {
			markings: [
				{ xaxis: { from: Date.now(), to: Date.now() }, color: '#ff0000' }
			]
		}
	}

	var placeholder = $('.plot-container div');

	/* update plot */
	plot = $.plot(placeholder, data, $.extend(true, options, plotOptions));

	placeholder.bind("plotselected", function (event, ranges) {
		$.each(plot.getXAxes(), function(_, axis) {
			var opts = axis.options;
			opts.min = ranges.xaxis.from;
			opts.max = ranges.xaxis.to;
		});

		plot.setupGrid();
		plot.draw();
		plot.clearSelection();
	});

	redrawPlot = false;
}

function wsConnect(node)
{
	var url = wsUrl(node.name);
	var conn = new WebSocket(url, 'live');

	conn.binaryType = 'arraybuffer';

	conn.onopen = function() {
		$('#status')
			.text('Connected')
			.css('color', 'green');

		console.log('WebSocket connection established');
	};

	conn.onclose = function(error) {
		console.log('WebSocket connection closed', error);

		$('#status')
			.text('Disconnected' + (error.reason != '' ? ' (' + error.reason + ')' : ''))
			.css('color', 'red');

		setTimeout(function() {
			wsConnect(currentNode);
		}, 1000);
	};

	conn.onerror = function(error) {
		console.log('WebSocket connection error', error);

		$('#status').text('Status: Error: ' + error.message);
	};

	conn.onmessage = function(e) {
		var msgs = Msg.fromArrayBufferVector(e.data);

		for (var j = 0; j < plotData.length; j++) {
			// remove old
			while (plotData[j].length > 0 && plotData[j][0][0] < (Date.now() - xPast))
				plotData[j].shift()
		}

		for (var j = 0; j < msgs.length; j++) {
			var msg = msgs[j];
			
			console.log('Received message with ' + msg.data.length + ' values from id ' + msg.id + ' with timestamp ' + new Date(msg.timestamp).toString() + ': '+ msg.data);

			// add empty arrays for data series
			while (plotData.length < msg.length)
				plotData.push([]);

			// add data to arrays
			for (var i = 0; i < msg.length; i++)
				plotData[i].push([msg.timestamp, msg.data[i]]);
		}

		redrawPlot = true;
	};

	return conn;
};

/* Helpers */
function wsUrl(endpoint)
{
	var l = window.location;
	var url = '';

	if (l.protocol == 'file:') {
		url += 'ws://localhost';
	}
	else {
		if (l.protocol === 'https:')
			url += 'wss://';
		else
			url += 'ws://';

		url += l.hostname;
	}

	if ((l.port) && (l.port != 80) && (l.port != 443))
		url += ':'+ l.port;

	url += '/' + endpoint;

	return url;
}

/* Some helpers */
