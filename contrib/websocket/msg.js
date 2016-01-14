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

var S2SS = S2SS || {};

/* Some constants for the binary protocol */
const Msg.VERSION		= 1;

const Msg.TYPE_DATA		= 0; /**< Message contains float values */
const Msg.TYPE_START		= 1; /**< Message marks the beginning of a new simulation case */
const Msg.TYPE_STOP		= 2; /**< Message marks the end of a simulation case */
const Msg.TYPE_EMPTY		= 3; /**< Message does not carry useful data */

const Msg.ENDIAN_LITTLE		= 0; /**< Message values are in little endian format (float too!) */
const Msg.ENDIAN_BIG		= 1; /**< Message values are in bit endian format */

/* Some offsets in the binary message */
const Msg.OFFSET_ENDIAN		= 1;
const Msg.OFFSET_TYPE		= 2;
const Msg.OFFSET_VERSION	= 4;

/* Class for parsing and printing a message / sample */
function Msg(members) {
	for(var k in members)
		this[k] = members[k];
}

Msg.prototype.length = function() {
	return this.length * 4 + 16;
}

Msg.prototype.toArrayBuffer = function() {
	var blob = new ArrayBuffer(this.length());
	
	return blob;
}

Msg.prototype.fromArrayBuffer = function(blob) {
	var hdr = new UInt32Array(blob, 0, 16);
	var hdr16 = new UInt16Array(blob, 0, 16);

	var msg = new Msg({
		endian:  (hdr[0] >> MSG_OFFSET_ENDIAN) & 0x1,
		version: (hdr[0] >> MSG_OFFSET_VERSION) & 0xF,
		type:    (hdr[0] >> MSG_OFFSET_TYPE) & 0x3,
		length:   hdr16[1],
		sequence: hdr[1],
		timestamp: 1e3 * (hdr[2] + hdr[3]), // in milliseconds
		blob : blob
	});

	if (msg.endian == MSG_ENDIAN_BIG) {
		console.warn("Unsupported endianness. Skipping message!");
		continue;

		/* @todo not working yet
		hdr = hdr.map(swap32);
		values = values.map(swap32);
		*/
	}
	
	
	msg.values = new Float32Array(msg, offset + 16, length * 4); // values reinterpreted as floats with 16byte offset in msg
}

Msg.prototype.fromArrayBufferVector = function(blob) {
	/* some local variables for parsing */
	var offset = 0;
	var msgs = [];

	/* for every msg in vector */
	while (offset < msg.byteLength) {
		var msg = Msg.fromArrayBuffer(ArrayBuffer(blob, offset));
		
		if (msg != undefined)
			msgs.push(msg);

		offset += msg.blobLength;
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

/** @} */