/** Javascript class for parsing binary messages
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup websocket
 * @ingroup node
 * @{
 **********************************************************************************/

//var S2SS = S2SS || {};

/* Class for parsing and printing a message / sample */
function Msg(members) {
	for(var k in members)
		this[k] = members[k];
}

/* Some constants for the binary protocol */
Msg.prototype.VERSION		= 1;

Msg.prototype.TYPE_DATA		= 0; /**< Message contains float values */
Msg.prototype.TYPE_START	= 1; /**< Message marks the beginning of a new simulation case */
Msg.prototype.TYPE_STOP		= 2; /**< Message marks the end of a simulation case */

Msg.prototype.ENDIAN_LITTLE	= 0; /**< Message values are in little endian format (float too!) */
Msg.prototype.ENDIAN_BIG	= 1; /**< Message values are in bit endian format */

/* Some offsets in the binary message */
Msg.prototype.OFFSET_ENDIAN	= 1;
Msg.prototype.OFFSET_TYPE	= 2;
Msg.prototype.OFFSET_VERSION	= 4;

Msg.prototype.values = function() {
	return this.values * 4 + 16;
}

Msg.prototype.toArrayBuffer = function() {
	var blob = new ArrayBuffer(this.values());
	
	return blob;
}

Msg.fromArrayBuffer = function(data) {
	var bits = data.getUint8(0);
	var endian = (bits >> Msg.OFFSET_ENDIAN) & 0x1 ? 0 : 1;

	var msg = new Msg({
		endian:  (bits >> Msg.OFFSET_ENDIAN) & 0x1,
		version: (bits >> Msg.OFFSET_VERSION) & 0xF,
		type:    (bits >> Msg.OFFSET_TYPE) & 0x3,
		values:    data.getUint16(0x02, endian),
		sequence:  data.getUint32(0x04, endian),
		timestamp: data.getUint32(0x08, endian) * 1e3 +
		           data.getUint32(0x0C, endian) * 1e-6,
	});
	
	msg.blob = new DataView(    data.buffer, data.byteOffset + 0x00, (msg.values + 4) * 4);
	msg.data = new Float32Array(data.buffer, data.byteOffset + 0x10, msg.values);

	if (msg.endian != host_endianess()) {
		console.warn("Message is not given in host endianess!");

		var data = new Uint32Array(msg.blob, 0x10);
		for (var i = 0; i < data.length; i++)
			data[i] = swap32(data[i]);
	}
	
	return msg;
}

Msg.fromArrayBufferVector = function(blob) {
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

/** @todo parsing of big endian messages not yet supported */
function swap16(val) {
    return ((val & 0xFF) << 8)
           | ((val >> 8) & 0xFF);
}

function swap32(val) {
    return ((val & 0xFF) << 24)
           | ((val & 0xFF00) << 8)
           | ((val >> 8) & 0xFF00)
           | ((val >> 24) & 0xFF);
}

function host_endianess() {
	var buffer = new ArrayBuffer(2);
	new DataView(buffer).setInt16(0, 256, true /* littleEndian */);
	return new Int16Array(buffer)[0] === 256 ? 0 : 1; // Int16Array uses the platform's endianness.
};

/** @} */