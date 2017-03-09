/** Javascript class for parsing binary messages
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/
/**
 * @addtogroup websocket
 * @ingroup node
 * @{
 *********************************************************************************/

/* Class for parsing and printing a message */
function Msg(c, d)
{
	this.sequence  = typeof c.sequence  === 'undefined' ? 0                           : c.sequence;
	this.length    = typeof c.length    === 'undefined' ? 0                           : c.length;
	this.endian    = typeof c.endian    === 'undefined' ? Msg.prototype.ENDIAN_LITTLE : c.endian;
	this.version   = typeof c.version   === 'undefined' ? Msg.prototype.VERSION       : c.version;
	this.type      = typeof c.type      === 'undefined' ? Msg.prototype.TYPE_DATA     : c.type;
	this.timestamp = typeof c.timestamp === 'undefined' ? Date.now()                  : c.timestamp;
	
	if (Array.isArray(d)) {
		this.length = d.length;
		this.data   = d
	}
}

/* Some constants for the binary protocol */
Msg.prototype.VERSION		= 1;

Msg.prototype.TYPE_DATA		= 0; /**< Message contains float values */

Msg.prototype.ENDIAN_LITTLE	= 0; /**< Message values are in little endian format (float too!) */
Msg.prototype.ENDIAN_BIG	= 1; /**< Message values are in bit endian format */

/* Some offsets in the binary message */
Msg.prototype.OFFSET_ENDIAN	= 1;
Msg.prototype.OFFSET_TYPE	= 2;
Msg.prototype.OFFSET_VERSION	= 4;

Msg.bytes = function(len)
{
	return len * 4 + 16;
}

Msg.fromArrayBuffer = function(data)
{
	var bits = data.getUint8(0);
	var endian = (bits >> Msg.prototype.OFFSET_ENDIAN) & 0x1 ? 0 : 1;

	var msg = new Msg({
		endian:  (bits >> Msg.prototype.OFFSET_ENDIAN) & 0x1,
		version: (bits >> Msg.prototype.OFFSET_VERSION) & 0xF,
		type:    (bits >> Msg.prototype.OFFSET_TYPE) & 0x3,
		length:    data.getUint16(0x02, endian),
		sequence:  data.getUint32(0x04, endian),
		timestamp: data.getUint32(0x08, endian) * 1e3 +
		           data.getUint32(0x0C, endian) * 1e-6,
	});
	
	msg.blob = new DataView(    data.buffer, data.byteOffset + 0x00, Msg.bytes(msg.length));
	msg.data = new Float32Array(data.buffer, data.byteOffset + 0x10, msg.length);

	if (msg.endian != host_endianess()) {
		console.warn("Message is not given in host endianess!");

		var data = new Uint32Array(msg.blob, 0x10);
		for (var i = 0; i < data.length; i++)
			data[i] = swap32(data[i]);
	}
	
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
	bits |= (this.endian  & 0x1) << Msg.prototype.OFFSET_ENDIAN;
	bits |= (this.version & 0xF) << Msg.prototype.OFFSET_VERSION;
	bits |= (this.type    & 0x3) << Msg.prototype.OFFSET_TYPE;
	
	var sec  = Math.floor(this.timestamp / 1e3);
	var nsec = (this.timestamp - sec * 1e3) * 1e6;
	
	view.setUint8( 0x00, bits, true);
	view.setUint16(0x02, this.length, true);
	view.setUint32(0x04, this.sequence, true);
	view.setUint32(0x08, sec, true);
	view.setUint32(0x0C, nsec, true);
	
	data = new Float32Array(buffer, 0x10, this.length);
	data.set(this.data);

	return buffer;
}

/** @todo parsing of big endian messages not yet supported */
function swap16(val)
{
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