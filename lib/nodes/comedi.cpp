/** Node type: comedi
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <comedilib.h>
#include <comedi_errno.h>

#include <villas/plugin.h>
#include <villas/nodes/comedi.hpp>
#include <villas/utils.hpp>

using namespace villas::utils;

/* Utility functions to dump a comedi_cmd graciously taken from comedilib demo */
static char* comedi_cmd_trigger_src(unsigned int src, char *buf);
static void comedi_dump_cmd(comedi_cmd *cmd, int debug_level);

static int comedi_parse_direction(struct comedi *c, struct comedi_direction *d, json_t *cfg)
{
	int ret;

	json_t *json_chans;
	json_error_t err;

	/* Default values */
	d->subdevice = -1;
	d->buffer_size = 16;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: i, s: o, s: i }",
		"subdevice", &d->subdevice,
		"bufsize", &d->buffer_size,
		"signals", &json_chans,
		"rate", &d->sample_rate_hz
	);
	if (ret)
		jerror(&err, "Failed to parse configuration");

	if (!json_is_array(json_chans))
		return -1;

	/* Convert kilobytes to bytes */
	d->buffer_size = d->buffer_size << 10;

	size_t i;
	json_t *json_chan;

	d->chanlist_len = json_array_size(json_chans);
	if (d->chanlist_len == 0) {
		error("No channels configured");
		return 0;
	}

	d->chanlist = (unsigned int*) alloc(d->chanlist_len * sizeof(*d->chanlist));
	assert(d->chanlist != nullptr);

	d->chanspecs = (comedi_chanspec *) alloc(d->chanlist_len * sizeof(*d->chanspecs));
	assert(d->chanspecs != nullptr);

	json_array_foreach(json_chans, i, json_chan) {
		int num, range, aref;
		ret = json_unpack_ex(json_chan, &err, 0, "{ s: i, s: i, s: i }",
		                     "channel", &num,
		                     "range", &range,
		                     "aref", &aref);
		if (ret)
			jerror(&err, "Failed to parse configuration");

		if (aref < AREF_GROUND || aref > AREF_OTHER)
			error("Invalid value for analog reference: aref=%d", aref);

		d->chanlist[i] = CR_PACK(num, range, aref);
	}

	return 0;
}

static int comedi_start_common(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction* directions[2] = { &c->in, &c->out };

	comedi_set_global_oor_behavior(COMEDI_OOR_NAN);

	for (int dirIdx = 0; dirIdx < 2; dirIdx++) {
		struct comedi_direction* d = directions[dirIdx];
		int ret;

		if (!d->present)
			continue;

		/* Sanity-check channel config and populate chanspec for later */
		for (unsigned i = 0; i < d->chanlist_len; i++) {
			const unsigned int channel = CR_CHAN(d->chanlist[i]);
			const int range = CR_RANGE(d->chanlist[i]);

			ret = comedi_get_n_ranges(c->dev, d->subdevice, channel);
			if (ret < 0)
				error("Failed to get ranges for channel %d on subdevice %d",
				      channel, d->subdevice);

			if (range >= ret)
				error("Invalid range for channel %d on subdevice %d: range=%d",
				      channel, d->subdevice, range);

			ret = comedi_get_maxdata(c->dev, d->subdevice, channel);
			if (ret <= 0)
				error("Failed to get max. data value for channel %d on subdevice %d",
				      channel, d->subdevice);

			d->chanspecs[i].maxdata = ret;
			d->chanspecs[i].range = comedi_get_range(c->dev, d->subdevice,
			                                         channel, range);

			info("%s channel: %d aref=%d range=%d maxdata=%d",
			     (d == &c->in ? "Input" : "Output"), channel,
			     CR_AREF(d->chanlist[i]), range, d->chanspecs[i].maxdata);
		}

		const int flags = comedi_get_subdevice_flags(c->dev, d->subdevice);
		d->sample_size = (flags & SDF_LSAMPL) ? sizeof(lsampl_t) : sizeof(sampl_t);

		/* Set buffer size */
		comedi_set_buffer_size(c->dev, d->subdevice, d->buffer_size);
		comedi_set_max_buffer_size(c->dev, d->subdevice, d->buffer_size);
		ret = comedi_get_buffer_size(c->dev, d->subdevice);
		if (ret != d->buffer_size)
			error("Failed to set buffer size for subdevice %d of node '%s'", d->subdevice, node_name(n));

		info("Set buffer size for subdevice %d to %d bytes", d->subdevice, d->buffer_size);

		ret = comedi_lock(c->dev, d->subdevice);
		if (ret)
			error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));
	}

	return 0;
}

static int comedi_start_in(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;

	/* Try to find first analog input subdevice if not specified in config */
	if (d->subdevice < 0) {
		d->subdevice = comedi_find_subdevice_by_type(c->dev, COMEDI_SUBD_AI, 0);
		if (d->subdevice < 0)
			error("Cannot find analog input device for node '%s'", node_name(n));
	}
	else {
		/* Check if subdevice is usable */
		ret = comedi_get_subdevice_type(c->dev, d->subdevice);
		if (ret != COMEDI_SUBD_AI)
			error("Input subdevice of node '%s' is not an analog input", node_name(n));
	}

	ret = comedi_get_subdevice_flags(c->dev, d->subdevice);
	if (ret < 0 || !(ret & SDF_CMD_READ))
		error("Input subdevice of node '%s' does not support 'read' commands", node_name(n));

	comedi_set_read_subdevice(c->dev, d->subdevice);
	ret = comedi_get_read_subdevice(c->dev);
	if (ret < 0 || ret != d->subdevice)
		error("Failed to change 'read' subdevice from %d to %d of node '%s'",
		      ret, d->subdevice, node_name(n));

	comedi_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.subdev = d->subdevice;

	/* Make card send interrupts after every sample, not only when fifo is half
	 * full (TODO: evaluate if this makes sense, leave as reminder) */
	//cmd.flags = TRIG_WAKE_EOS;

	/* Start right now */
	cmd.start_src = TRIG_NOW;

	/* Trigger scans periodically */
	cmd.scan_begin_src = TRIG_TIMER;
	cmd.scan_begin_arg = 1e9 / d->sample_rate_hz;

	/* Do conversions in serial with 1ns inter-conversion delay */
	cmd.convert_src = TRIG_TIMER;
	cmd.convert_arg = 1;	/* Inter-conversion delay in nanoseconds */

	/* Terminate scan after each channel has been converted */
	cmd.scan_end_src = TRIG_COUNT;
	cmd.scan_end_arg = d->chanlist_len;

	/* Contionous sampling */
	cmd.stop_src = TRIG_NONE;

	cmd.chanlist = d->chanlist;
	cmd.chanlist_len = d->chanlist_len;

	/* First run might change command, second should return successfully */
	ret = comedi_command_test(c->dev, &cmd);
	ret = comedi_command_test(c->dev, &cmd);
	if (ret < 0)
		error("Invalid command for input subdevice of node '%s'", node_name(n));

	info("Input command:");
	comedi_dump_cmd(&cmd, 1);

	ret = comedi_command(c->dev, &cmd);
	if (ret < 0)
		error("Failed to issue command to input subdevice of node '%s'", node_name(n));

	d->started = time_now();
	d->counter = 0;
	d->running = true;

#if COMEDI_USE_READ
	/* Be prepared to consume one entire buffer */
	c->buf = (char *) alloc(c->in.buffer_size);
	c->bufptr = c->buf;
	assert(c->bufptr != nullptr);

	info("Compiled for kernel read() interface");
#else
	info("Compiled for kernel mmap() interface");
#endif

	return 0;
}

static int comedi_start_out(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->out;

	/* Try to find first analog output subdevice if not specified in config */
	if (d->subdevice < 0) {
		d->subdevice = comedi_find_subdevice_by_type(c->dev, COMEDI_SUBD_AO, 0);
		if (d->subdevice < 0)
			error("Cannot find analog output device for node '%s'", node_name(n));
	}
	else {
		ret = comedi_get_subdevice_type(c->dev, d->subdevice);
		if (ret != COMEDI_SUBD_AO)
			error("Output subdevice of node '%s' is not an analog output", node_name(n));
	}

	ret = comedi_get_subdevice_flags(c->dev, d->subdevice);
	if (ret < 0 || !(ret & SDF_CMD_WRITE))
		error("Output subdevice of node '%s' does not support 'write' commands", node_name(n));

	comedi_set_write_subdevice(c->dev, d->subdevice);
	ret = comedi_get_write_subdevice(c->dev);
	if (ret < 0 || ret != d->subdevice)
		error("Failed to change 'write' subdevice from %d to %d of node '%s'",
		      ret, d->subdevice, node_name(n));

	comedi_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.subdev = d->subdevice;

	cmd.flags = CMDF_WRITE;

	/* Wait for internal trigger, we will have to fill the buffer first */
	cmd.start_src = TRIG_INT;
	cmd.start_arg = 0;

	cmd.scan_begin_src = TRIG_TIMER;
	cmd.scan_begin_arg = 1e9 / d->sample_rate_hz;

	cmd.convert_src = TRIG_NOW;
	cmd.convert_arg = 0;

	cmd.scan_end_src = TRIG_COUNT;
	cmd.scan_end_arg = d->chanlist_len;

	/* Continous sampling */
	cmd.stop_src = TRIG_NONE;
	cmd.stop_arg = 0;

	cmd.chanlist = d->chanlist;
	cmd.chanlist_len = d->chanlist_len;

	/* First run might change command, second should return successfully */
	ret = comedi_command_test(c->dev, &cmd);
	if (ret < 0)
		error("Invalid command for input subdevice of node '%s'", node_name(n));

	ret = comedi_command_test(c->dev, &cmd);
	if (ret < 0)
		error("Invalid command for input subdevice of node '%s'", node_name(n));

	info("Output command:");
	comedi_dump_cmd(&cmd, 1);

	ret = comedi_command(c->dev, &cmd);
	if (ret < 0)
		error("Failed to issue command to input subdevice of node '%s'", node_name(n));

	/* Output will only start after the internal trigger */
	d->running = false;
	d->last_debug = time_now();

	/* Allocate buffer for one complete villas sample */
	/** @todo: maybe increase buffer size according to c->vectorize */
	const size_t local_buffer_size = d->sample_size * d->chanlist_len;
	d->buffer = (char *) alloc(local_buffer_size);
	d->bufptr = d->buffer;
	assert(d->buffer != nullptr);

	/* Initialize local buffer used for write() syscalls */
	for (unsigned channel = 0; channel < d->chanlist_len; channel++) {
		const unsigned raw = comedi_from_phys(0.0f, d->chanspecs[channel].range, d->chanspecs[channel].maxdata);

		if (d->sample_size == sizeof(sampl_t))
			*((sampl_t *)d->bufptr) = raw;
		else
			*((lsampl_t *)d->bufptr) = raw;

		d->bufptr += d->sample_size;
	}

	/* Preload comedi output buffer */
	for (unsigned i = 0; i < d->buffer_size / local_buffer_size; i++) {
		size_t written = write(comedi_fileno(c->dev), d->buffer, local_buffer_size);
		if (written != local_buffer_size) {
			error("Cannot preload Comedi buffer");
		}
	}

	const size_t villas_samples_in_kernel_buf = d->buffer_size / (d->sample_size * d->chanlist_len);
	const double latencyMs = (double)villas_samples_in_kernel_buf / d->sample_rate_hz * 1e3;
	info("Added latency due to buffering: %4.1f ms\n", latencyMs);

	return 0;
}

static int comedi_stop_in(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;

	comedi_cancel(c->dev, d->subdevice);

	ret = comedi_unlock(c->dev, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

	return 0;
}

static int comedi_stop_out(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->out;

	comedi_cancel(c->dev, d->subdevice);

	ret = comedi_unlock(c->dev, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

	return 0;
}

int comedi_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	const char *device;

	json_t *json_in = nullptr;
	json_t *json_out = nullptr;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: o, s?: o }",
	                     "device", &device,
	                     "in", &json_in,
	                     "out", &json_out);

	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	c->in.present = json_in != nullptr;
	c->in.enabled = false;
	c->in.running = false;

	c->out.present = json_out != nullptr;
	c->out.enabled = false;
	c->out.running = false;

	if (c->in.present) {
		ret = comedi_parse_direction(c, &c->in, json_in);
		if (ret)
			return ret;
	}

	if (c->out.present) {
		ret = comedi_parse_direction(c, &c->out, json_out);
		if (ret)
			return ret;
	}

	c->device = strdup(device);

	return 0;
}

char * comedi_print(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	char *buf = nullptr;

	const char *board = comedi_get_board_name(c->dev);
	const char *driver = comedi_get_driver_name(c->dev);

	strcatf(&buf, "board=%s, driver=%s, device=%s", board, driver, c->device);

	return buf;
}

int comedi_start(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	c->dev = comedi_open(c->device);
	if (!c->dev) {
		const char *err = comedi_strerror(comedi_errno());
		error("Failed to open device: %s", err);
	}

	/* Enable non-blocking syscalls */
	/** @todo: verify if this works with both input and output, so comment out */
	//if (fcntl(comedi_fileno(c->dev), F_SETFL, O_NONBLOCK))
	//	error("Failed to set non-blocking flag in Comedi FD of node '%s'", node_name(n));

	comedi_start_common(n);

	if (c->in.present) {
		int ret = comedi_start_in(n);
		if (ret)
			return ret;

		c->in.enabled = true;
	}

	if (c->out.present) {
		int ret = comedi_start_out(n);
		if (ret)
			return ret;

		c->out.enabled = true;
	}

#if !COMEDI_USE_READ
	info("Mapping Comedi buffer of %d bytes", c->in.buffer_size);
	c->map = mmap(nullptr, c->in.buffer_size, PROT_READ, MAP_SHARED, comedi_fileno(c->dev), 0);
	if (c->map == MAP_FAILED)
		error("Failed to map comedi buffer of node '%s'", node_name(n));

	c->front = 0;
	c->back = 0;
	c->bufpos = 0;
#endif

	return 0;
}

int comedi_stop(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	if (c->in.enabled)
		comedi_stop_in(n);

	if (c->out.enabled)
		comedi_stop_out(n);

	ret = comedi_close(c->dev);
	if (ret)
		return ret;

	return 0;
}

#if COMEDI_USE_READ

int comedi_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;
	const size_t villas_sample_size = d->chanlist_len * d->sample_size;

	ret = comedi_get_buffer_contents(c->dev, d->subdevice);
	if (ret < 0) {
		if (comedi_errno() == EBUF_OVR)
			error("Comedi buffer overflow");
		else
			error("Comedi error: %s", comedi_strerror(comedi_errno()));
	}

	fd_set rdset;
	FD_ZERO(&rdset);
	FD_SET(comedi_fileno(c->dev), &rdset);

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 5000;

	ret = select(comedi_fileno(c->dev) + 1, &rdset, nullptr, nullptr, &timeout);
	if (ret < 0)
		error("select");
	else if (ret == 0) /* hit timeout */
		return 0;
	else if (FD_ISSET(comedi_fileno(c->dev), &rdset)) { /* comedi file descriptor became ready */
		const size_t buffer_bytes_free = d->buffer_size - (c->bufptr - c->buf);
		const size_t bytes_requested = cnt * villas_sample_size;

		ret = read(comedi_fileno(c->dev), c->bufptr, MIN(bytes_requested, buffer_bytes_free));
		if (ret < 0) {
			if (errno == EAGAIN)
				error("read");
			else
				return 0;
		}
		else if (ret == 0) {
			warning("select timeout, no samples available");
			return 0;
		}
		else {
			/* Sample handling here */
			const size_t bytes_available = ret;
			const size_t raw_samples_available = bytes_available / d->sample_size;
			const size_t villas_samples_available = raw_samples_available / d->chanlist_len;

			info("there are %ld bytes available (%ld requested) => %ld villas samples",
			     bytes_available, bytes_requested, villas_samples_available);

			info("there are %ld kB available (%ld kB requested)",
			     bytes_available / 1024, bytes_requested / 1024);

			if (cnt > villas_samples_available)
				cnt = villas_samples_available;

			for (size_t i = 0; i < cnt; i++) {
				d->counter++;

				smps[i]->flags = SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE;
				smps[i]->sequence = d->counter / d->chanlist_len;

				struct timespec offset = time_from_double(d->counter * 1.0 / d->sample_rate_hz);
				smps[i]->ts.origin = time_add(&d->started, &offset);

				smps[i]->length = d->chanlist_len;

				if (smps[i]->capacity < d->chanlist_len) {
					error("Sample has insufficient capacity: %d < %ld",
					      smps[i]->capacity, d->chanlist_len);
				}

				for (unsigned si = 0; si < d->chanlist_len; si++) {
					unsigned int raw;

					if (d->sample_size == sizeof(sampl_t))
						raw = *((sampl_t *)(c->bufptr));
					else
						raw = *((lsampl_t *)(c->bufptr));

					c->bufptr += d->sample_size;

					smps[i]->data[si].f = comedi_to_phys(raw, d->chanspecs[si].range, d->chanspecs[si].maxdata);

					if (isnan(smps[i]->data[si].f))
						warning("Input: channel %d clipped", CR_CHAN(d->chanlist[si]));
				}
			}

			const size_t bytes_consumed = cnt * villas_sample_size;
			const size_t bytes_left = bytes_available - bytes_consumed;
			if (bytes_left > 0) {
				/* Move leftover bytes to the beginning of buffer */
				/** @todo: optimize? */
				memmove(c->buf, c->bufptr, bytes_left);
			}

			info("consumed %ld bytes", bytes_consumed);

			/* Start at the beginning again */
			c->bufptr = c->buf;

			return cnt;
		}
	}
	else {
		/* unknown file descriptor became ready */
		printf("unknown file descriptor ready\n");
	}

	return -1;
}

#else

int comedi_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;

	const size_t villas_sample_size = d->chanlist_len * d->sample_size;

	comedi_set_read_subdevice(c->dev, d->subdevice);

	info("current bufpos=%ld", c->bufpos);

#if 0
	if (c->bufpos > (d->buffer_size - villas_sample_size)) {
		ret = comedi_get_buffer_read_offset(c->dev, d->subdevice);
		if (ret < 0)
			error("Canot get offset");

		c->bufpos = ret;
		info("change bufpos=%ld", c->bufpos);
	}
#endif

	ret = comedi_get_buffer_contents(c->dev, d->subdevice);
	if (ret == 0)
		return 0;
	else if (ret < 0) {
		if (comedi_errno() == EBUF_OVR)
			error("Comedi buffer overflow");
		else
			error("Comedi error: %s", comedi_strerror(comedi_errno()));
	}

	const size_t bytes_available = ret;
	const size_t raw_sample_count = bytes_available / d->sample_size;
	size_t villas_sample_count = raw_sample_count / d->chanlist_len;
	if (villas_sample_count == 0)
		return 0;

	info("there are %ld villas samples (%ld raw bytes, %ld channels)", villas_sample_count, bytes_available, d->chanlist_len);

#if 0
	if (villas_sample_count == 1)
		info("front=%ld back=%ld bufpos=%ld", c->front, c->back, c->bufpos);

	if ((c->bufpos + bytes_available) >= d->buffer_size) {
		/* Let comedi do the wraparound, only consume until end of buffer */
		villas_sample_count = (d->buffer_size - c->bufpos) / villas_sample_size;
		warning("Reducing consumption from %d to %ld bytes", ret, bytes_available);
		warning("Only consume %ld villas samples b/c of buffer wraparound", villas_sample_count);
	}
#endif

	if (cnt > villas_sample_count)
		cnt = villas_sample_count;

#if 0
	if (bytes_available != 0 && bytes_available < villas_sample_size) {
		warning("Cannot consume samples, only %d bytes available, throw away", ret);

		ret = comedi_mark_buffer_read(c->dev, d->subdevice, bytes_available);
		if (ret != bytes_available)
			error("Cannot throw away %ld bytes, returned %d, wtf comedi?!",
			      bytes_available, ret);

		return 0;
	}
#endif

	const size_t samples_total_bytes = cnt * villas_sample_size;

	ret = comedi_mark_buffer_read(c->dev, d->subdevice, samples_total_bytes);
	if (ret == 0) {
		warning("Marking read buffer (%ld bytes) not working, try again later", samples_total_bytes);
		return 0;
	}
	else if (ret != samples_total_bytes) {
		warning("Can only mark %d bytes as read, reducing samples", ret);
		return 0;
	}
	else
		info("Consume %d bytes", ret);

	/* Align front to whole samples */
	c->front = c->back + samples_total_bytes;

	for (size_t i = 0; i < cnt; i++) {
		d->counter++;

		smps[i]->flags = SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE;
		smps[i]->sequence = d->counter / d->chanlist_len;

		struct timespec offset = time_from_double(d->counter * 1.0 / d->sample_rate_hz);
		smps[i]->ts.origin = time_add(&d->started, &offset);

		smps[i]->length = d->chanlist_len;

		if (smps[i]->capacity < d->chanlist_len)
			error("Sample has insufficient capacity: %d < %ld",
			      smps[i]->capacity, d->chanlist_len);

		for (int si = 0; si < d->chanlist_len; si++) {
			unsigned int raw;

			if (d->sample_size == sizeof(sampl_t))
				raw = *((sampl_t *)(c->map + c->bufpos));
			else
				raw = *((lsampl_t *)(c->map + c->bufpos));

			smps[i]->data[si].f = comedi_to_phys(raw, d->chanspecs[si].range, d->chanspecs[si].maxdata);

			if (isnan(smps[i]->data[si].f)) {
				error("got nan");
			}

//			smps[i]->data[si].i = raw;

			c->bufpos += d->sample_size;
			if (c->bufpos >= d->buffer_size) {
				warning("read buffer wraparound");
//				c->bufpos = 0;
			}
		}
	}

//	const size_t bytes_consumed = c->front - c->back;

//	info("advance comedi buffer by %ld bytes", bytes_consumed);

	ret = comedi_get_buffer_read_offset(c->dev, d->subdevice);
	if (ret < 0) {
		if (comedi_errno() != EPIPE)
			error("Failed to get read buffer offset: %d, comedi errno %d", ret, comedi_errno());
		else
			ret = c->bufpos;
	}

	warning("change bufpos: %ld to %d", c->bufpos, ret);
	c->bufpos = ret;

#if 0
	ret = comedi_mark_buffer_read(c->dev, d->subdevice, bytes_consumed);
	if (ret < 0) //!= bytes_consumed)
		error("Failed to mark buffer position (ret=%d) for input stream of node '%s'", ret, node_name(n));
//	else if (ret == 0) {
	else {
		info("consumed %ld bytes", bytes_consumed);
		info("mark buffer returned %d", ret);

		if (ret == 0) {
			ret = comedi_mark_buffer_read(c->dev, d->subdevice, bytes_consumed);
			info("trying again, mark buffer returned now %d", ret);
		}

		if (ret > 0) {
			ret = comedi_get_buffer_read_offset(c->dev, d->subdevice);
			if (ret < 0)
				error("Failed to get read buffer offset");

			warning("change bufpos1: %ld to %d", c->bufpos, ret);
			c->bufpos = ret;
		}
		else {
//			warning("change bufpos2: %ld to %ld", c->bufpos, c->);
//			c->bufpos += bytes_consumed;
			warning("keep bufpos=%ld", c->bufpos);
		}

//		c->bufpos = 0;
	}
#endif

//	info("new bufpos: %ld", c->bufpos);

	c->back = c->front;

	return cnt;
}

#endif

int comedi_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->out;

	if (!d->enabled) {
		warning("Attempting to write, but output is not enabled");
		return 0;
	}

	if (!d->running) {
		/* Output was not yet running, so start now */
		ret = comedi_internal_trigger(c->dev, d->subdevice, 0);
		if (ret < 0)
			error("Failed to trigger-start output");

		d->started = time_now();
		d->counter = 0;
		d->running = true;

		info("Starting output of node '%s'", node_name(n));
	}

	const size_t buffer_capacity_raw = d->buffer_size / d->sample_size;
	const size_t buffer_capacity_villas = buffer_capacity_raw / d->chanlist_len;
	const size_t villas_sample_size = d->sample_size * d->chanlist_len;

	ret = comedi_get_buffer_contents(c->dev, d->subdevice);
	if (ret < 0) {
		if (comedi_errno() == EBUF_OVR)
			error("Comedi buffer overflow");
		else
			error("Comedi error: %s", comedi_strerror(comedi_errno()));
	}

	const size_t bytes_in_buffer = ret;
	const size_t raw_samples_in_buffer = bytes_in_buffer / d->sample_size;
	const size_t villas_samples_in_buffer = raw_samples_in_buffer / d->chanlist_len;

	if (villas_samples_in_buffer == buffer_capacity_villas) {
		warning("Comedi buffer is full");
		return 0;
	}
	else {
		struct timespec now = time_now();
		if (time_delta(&d->last_debug, &now) >= 1) {
			debug(LOG_COMEDI | 2, "Comedi write buffer: %4ld villas samples (%2.0f%% of buffer)",
			      villas_samples_in_buffer,
			      (100.0f * villas_samples_in_buffer / buffer_capacity_villas));

			d->last_debug = time_now();
		}
	}

	size_t villas_samples_written = 0;

	while (villas_samples_written < cnt) {
		struct sample *sample = smps[villas_samples_written];
		if (sample->length != d->chanlist_len)
			error("Value count in sample (%d) != configured output channels (%ld)",
			      sample->length, d->chanlist_len);

		d->bufptr = d->buffer;

		/* Move samples from villas into local buffer for comedi */
		for (unsigned si = 0; si < sample->length; si++) {
			unsigned raw_value = 0;

			switch (sample_format(sample, si)) {
				case SIGNAL_TYPE_FLOAT:
					raw_value = comedi_from_phys(sample->data[si].f, d->chanspecs[si].range, d->chanspecs[si].maxdata);
					break;

				case SIGNAL_TYPE_INTEGER:
					/* Treat sample as already raw DAC value */
					raw_value = sample->data[si].i;
					break;

				case SIGNAL_TYPE_BOOLEAN:
					raw_value = comedi_from_phys(sample->data[si].b ? 1 : 0, d->chanspecs[si].range, d->chanspecs[si].maxdata);
					break;

				case SIGNAL_TYPE_COMPLEX:
					/* We only output the real part */
					raw_value = comedi_from_phys(creal(sample->data[si].z), d->chanspecs[si].range, d->chanspecs[si].maxdata);
					break;

				case SIGNAL_TYPE_INVALID:
					raw_value = 0;
					break;
			}

			if (d->sample_size == sizeof(sampl_t))
				*((sampl_t *)d->bufptr) = raw_value;
			else
				*((lsampl_t *)d->bufptr) = raw_value;

			d->bufptr += d->sample_size;
		}

		/* Try to write one complete villas sample to comedi */
		size_t written = write(comedi_fileno(c->dev), d->buffer, villas_sample_size);
		if (written < 0)
			error("write");
		else if (written == 0)
			break;	/* Comedi doesn't accept any more samples at the moment */
		else if (written == villas_sample_size)
			villas_samples_written++;
		else
			error("Only partial sample written (%zu bytes), oops", written);
	}

	if (villas_samples_written == 0) {
		warning("Nothing done");
	}

	d->counter += villas_samples_written;

	return villas_samples_written;
}

char* comedi_cmd_trigger_src(unsigned int src, char *buf)
{
	buf[0] = 0;

	if (src & TRIG_NONE)		strcat(buf, "none|");
	if (src & TRIG_NOW)		strcat(buf, "now|");
	if (src & TRIG_FOLLOW)	strcat(buf, "follow|");
	if (src & TRIG_TIME)		strcat(buf, "time|");
	if (src & TRIG_TIMER)	strcat(buf, "timer|");
	if (src & TRIG_COUNT)	strcat(buf, "count|");
	if (src & TRIG_EXT)		strcat(buf, "ext|");
	if (src & TRIG_INT)		strcat(buf, "int|");
#ifdef TRIG_OTHER
	if (src & TRIG_OTHER)	strcat(buf, "other|");
#endif

	if (strlen(buf) == 0)
		sprintf(buf, "unknown(0x%08x)", src);
	else
		buf[strlen(buf) - 1] = 0;

	return buf;
}

void comedi_dump_cmd(comedi_cmd *cmd, int debug_level)
{
	char buf[256];
	char* src;

	debug(LOG_COMEDI | debug_level, "subdevice:           %u", cmd->subdev);

	src = comedi_cmd_trigger_src(cmd->start_src, buf);
	debug(LOG_COMEDI | debug_level, "start:      %-8s %u", src, cmd->start_arg);

	src = comedi_cmd_trigger_src(cmd->scan_begin_src, buf);
	debug(LOG_COMEDI | debug_level, "scan_begin: %-8s %u", src, cmd->scan_begin_arg);

	src = comedi_cmd_trigger_src(cmd->convert_src, buf);
	debug(LOG_COMEDI | debug_level, "convert:    %-8s %u", src, cmd->convert_arg);

	src = comedi_cmd_trigger_src(cmd->scan_end_src, buf);
	debug(LOG_COMEDI | debug_level, "scan_end:   %-8s %u", src, cmd->scan_end_arg);

	src = comedi_cmd_trigger_src(cmd->stop_src,buf);
	debug(LOG_COMEDI | debug_level, "stop:       %-8s %u", src, cmd->stop_arg);
}

int comedi_poll_fds(struct node *n, int fds[])
{
	struct comedi *c = (struct comedi *) n->_vd;

	fds[0] = comedi_fileno(c->dev);

	return 0;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == STATE_DESTROYED)
		vlist_init(&plugins);

	p.name			= "comedi";
	p.description		= "Comedi-compatible DAQ/ADC cards";
	p.type			= PLUGIN_TYPE_NODE;
	p.node.instances.state	= STATE_DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct comedi);
	p.node.parse		= comedi_parse;
	p.node.print		= comedi_print;
	p.node.start		= comedi_start;
	p.node.stop		= comedi_stop;
	p.node.read		= comedi_read;
	p.node.write		= comedi_write;
	p.node.poll_fds		= comedi_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != STATE_DESTROYED)
		vlist_remove_all(&plugins, &p);
}
