/** Javascript class for parsing binary messages.
 *
 * Note: Messages sent by the 'websocket' node-type are always send in little endian byte-order!
 *       This is different from the messages send with the 'socket' node-type!
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup websocket
 * @ingroup node
 * @{
 */

/* Class for parsing and printing a message */
function Msg(c)
{
	this.sequence  = typeof c.sequence  === 'undefined' ? 0                           : c.sequence;
	this.length    = typeof c.length    === 'undefined' ? 0                           : c.length;
	this.version   = typeof c.version   === 'undefined' ? Msg.prototype.VERSION       : c.version;
	this.type      = typeof c.type      === 'undefined' ? Msg.prototype.TYPE_DATA     : c.type;
	this.id        = typeof c.id        === 'undefined' ? -1                          : c.id;
	this.timestamp = typeof c.timestamp === 'undefined' ? Date.now()                  : c.timestamp;
	
	if (Array.isArray(c.data)) {
		this.length = c.data.length;
		this.data   = c.data;
	}
}

/* Some constants for the binary protocol */
Msg.prototype.VERSION		= 1;

Msg.prototype.TYPE_DATA		= 0; /**< Message contains float values */

/* Some offsets in the binary message */
Msg.prototype.OFFSET_TYPE	= 2;
Msg.prototype.OFFSET_VERSION	= 4;

Msg.bytes = function(len)
{
	return len * 4 + 16;
}

Msg.fromArrayBuffer = function(data)
{
	var bits = data.getUint8(0);

	var msg = new Msg({
		version: (bits >> Msg.prototype.OFFSET_VERSION) & 0xF,
		type:    (bits >> Msg.prototype.OFFSET_TYPE) & 0x3,
		id:        data.getUint8( 0x01),
		length:    data.getUint16(0x02, 1),
		sequence:  data.getUint32(0x04, 1),
		timestamp: data.getUint32(0x08, 1) * 1e3 +
		           data.getUint32(0x0C, 1) * 1e-6,
	});
	
	msg.blob = new DataView(    data.buffer, data.byteOffset + 0x00, Msg.bytes(msg.length));
	msg.data = new Float32Array(data.buffer, data.byteOffset + 0x10, msg.length);

	return msg;
}

Msg.fromArrayBufferVector = function(blob)
{
	/* some local variables for parsing */
	var offset = 0;
	var msgs = [];

	/* for every msg in vector */
	while (offset < blob.byteLength) {
		var msg = Msg.fromArrayBuffer(new DataView(blob, offset));
		
		if (msg != undefined) {
			msgs.push(msg);

			offset += msg.blob.byteLength;
		}
	}

	return msgs;
}

Msg.prototype.toArrayBuffer = function()
{
	buffer = new ArrayBuffer(Msg.bytes(this.length))
	view   = new DataView(buffer);
	
	var bits = 0;
	bits |= (this.version & 0xF) << Msg.prototype.OFFSET_VERSION;
	bits |= (this.type    & 0x3) << Msg.prototype.OFFSET_TYPE;
	
	var sec  = Math.floor(this.timestamp / 1e3);
	var nsec = (this.timestamp - sec * 1e3) * 1e6;
	
	view.setUint8( 0x00, bits, true);
	view.setUint8( 0x01, this.id, true);
	view.setUint16(0x02, this.length, true);
	view.setUint32(0x04, this.sequence, true);
	view.setUint32(0x08, sec, true);
	view.setUint32(0x0C, nsec, true);
	
	data = new Float32Array(buffer, 0x10, this.length);
	data.set(this.data);

	return buffer;
}

/** @} */