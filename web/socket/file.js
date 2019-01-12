/** Read from a local file.
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

function fileStart(e)
{
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

		console.log('Read ' + msgs.length + ' samples from file');

		if (msgs.length > 0) {
			var offset = Date.now() - msgs[0].ts;
			var data = [];

			for (var i = 0; i < msgs.length; i++)
				data.push(msgs[i].ts + offset, msgs[i].data[0]);

			plotData.push(data);
		}
		else {

		}
	};

	reader.readAsText(file);
}
