/** Remote API.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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