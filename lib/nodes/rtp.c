/** Node type: rtp
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include <re/re_types.h>
#include <re/re_main.h>
#include <re/re_mbuf.h>
#include <re/re_mem.h>
#include <re/re_rtp.h>
#include <re/re_sys.h>
#undef ALIGN_MASK

#include <villas/plugin.h>
#include <villas/nodes/socket.h>
#include <villas/nodes/rtp.h>
#include <villas/utils.h>
#include <villas/format_type.h>

pthread_t re_pthread;

int rtp_reverse(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;
	struct sa tmp;

	tmp = r->local;
	r->local = r->remote;
	r->remote = tmp;

	return 0;
}

int rtp_parse(struct node *n, json_t *cfg)
{
	int ret = 0;
	struct rtp *r = (struct rtp *) n->_vd;

	const char *local, *remote;
	const char *format = "villas.binary";
	bool enable_rtcp = false;

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s: { s: s }, s: { s: s } }",
		"format", &format,
		"enable_rtcp", &enable_rtcp,
		"out",
			"address", &remote,
		"in",
			"address", &local
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	/* Format */
	r->format = format_type_lookup(format);
	if(!r->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	/* Enable RTCP */
	r->enable_rtcp = enable_rtcp;
	if(enable_rtcp)
		error("RTCP is not implemented yet.");

	/* Remote address */
	ret = sa_decode(&r->remote, remote, strlen(remote));
	if (ret) {
		error("Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), strerror(ret));
	}

	/* Local address */
	ret = sa_decode(&r->local, local, strlen(local));
	if (ret) {
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), strerror(ret));
	}

	/** @todo parse * in addresses */

	return ret;
}

char * rtp_print(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;
	char *buf;

	char *local = socket_print_addr((struct sockaddr *) &r->local.u);
	char *remote = socket_print_addr((struct sockaddr *) &r->remote.u);

	buf = strf("format=%s, in.address=%s, out.address=%s", format_type_name(r->format), local, remote);

	free(local);
	free(remote);

	return buf;
}

static void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
	struct rtp *r = (struct rtp *) arg;

	if (queue_push(&r->recv_queue, (void *) mbuf_alloc_ref(mb)) != 1)
		warn("Failed to push to queue");

	/* source, header not yet used */
	(void) src;
	(void) hdr;
}

int rtp_start(struct node *n)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	/* Initialize Queue */
	ret = queue_init(&r->recv_queue, 8, &memory_heap);
	if (ret)
		return ret;

	/* Initialize IO */
	ret = io_init(&r->io, r->format, &n->signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&r->io);
	if (ret)
		return ret;

	/* Initialize RTP socket */
	uint16_t port = sa_port(&r->local) & ~1;
	ret = rtp_listen(&r->rs, IPPROTO_UDP, &r->local, port, port+1, r->enable_rtcp, rtp_handler, NULL, n->_vd);

	return ret;
}

int rtp_stop(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;

	/*mem_deref(r->rs);*/

	return io_destroy(&r->io);
}

static void * th_func(void *arg)
{
	re_main(NULL);
	return NULL;
}

int rtp_type_start()
{
	int ret;

	/* Initialize library */
	ret = libre_init();
	if (ret) {
		error("Error initializing libre");
		return ret;
	}

	/* Add worker thread */
	ret = pthread_create(&re_pthread, NULL, th_func, NULL);
	if (ret) {
		error("Error creating rtp node type pthread");
		return ret;
	}

	return ret;
}

int rtp_type_stop()
{
	int ret;

	/* Join worker thread */
	re_cancel();
	pthread_cancel(re_pthread); /* @todo avoid using pthread_cancel */
	ret = pthread_join(re_pthread, NULL);
	if (ret) {
		error("Error joining rtp node type pthread");
		return ret;
	}

	libre_close();
	return ret;
}

int rtp_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;
	size_t bytes;
	char *buf;
	struct mbuf *mb;

	/* Get data from queue */
	ret = queue_pull(&r->recv_queue, (void **) &mb);
	if (ret <= 0) {
		if (ret < 0)
			warn("Failed to pull from queue");
		return ret;
	}

	/* Read from mbuf */
	bytes = mbuf_get_left(mb);
	buf = (char *) alloc(bytes);
	mbuf_read_mem(mb, (uint8_t *) buf, bytes);

	/* Unpack data */
	ret = io_sscan(&r->io, buf, bytes, NULL, smps, cnt);
	if (ret < 0)
		warn("Received invalid packet from node %s: reason=%d", node_name(n), ret);

	free(buf);
	return ret;
}

int rtp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	char *buf;
	char pad[] = "            ";
	size_t buflen;
	size_t wbytes;

	buflen = RTP_INITIAL_BUFFER_LEN;
	buf = alloc(buflen);
	if (!buf) {
		error("Error allocating buffer space");
		return -1;
	}

retry:	cnt = io_sprint(&r->io, buf, buflen, &wbytes, smps, cnt);
	if (cnt < 0) {
		error("Error from io_sprint, reason: %d", cnt);
		goto out;
	}

	if (wbytes <= 0) {
		error("Error written bytes = %ld <= 0", wbytes);
		goto out;
	}

	if (wbytes > buflen) {
		buflen = wbytes;
		buf = realloc(buf, buflen);
		goto retry;
	}

	/* Prepare mbuf */
	struct mbuf *mb = mbuf_alloc(buflen + 12);
	ret = mbuf_write_str(mb, pad);
	if (ret) {
		error("Error writing padding to mbuf");
		cnt = ret;
		goto out;
	}
	ret = mbuf_write_mem(mb, (uint8_t*)buf, buflen);
	if (ret) {
		error("Error writing data to mbuf");
		cnt = ret;
		goto out;
	}
	mbuf_set_pos(mb, 12);

	/* Send dataset */
	ret = rtp_send(r->rs, &r->remote, false, false, 21, (uint32_t) time(NULL), mb);
	if (ret) {
		error("Error from rtp_send, reason: %d", ret);
		cnt = ret;
	}

out:	free(buf);
		mem_deref(mb);

	return cnt;
}

int rtp_fd(struct node *n)
{
	/* struct rtp *r = (struct rtp *) n->_vd; */

	error("No access to file descriptor.");

	return -1;
}

static struct plugin p = {
	.name		= "rtp",
	.description	= "real-time transport protocol (libre)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct rtp),
		.type.start = rtp_type_start,
		.type.stop	= rtp_type_stop,
		.reverse	= rtp_reverse,
		.parse		= rtp_parse,
		.print		= rtp_print,
		.start		= rtp_start,
		.stop		= rtp_stop,
		.read		= rtp_read,
		.write		= rtp_write,
		.fd		= rtp_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
