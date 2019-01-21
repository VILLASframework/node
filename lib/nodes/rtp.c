/** Node type: rtp
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

#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include <re/re_types.h>
#include <re/re_main.h>
#include <re/re_mbuf.h>
#include <re/re_mem.h>
#include <re/re_rtp.h>
#include <re/re_sys.h>
#undef ALIGN_MASK

#include <villas/plugin.h>
#include <villas/nodes/socket.h>
#include <villas/hooks/limit_rate.h>
#include <villas/hooks/decimate.h>
#include <villas/nodes/rtp.h>
#include <villas/utils.h>
#include <villas/hook.h>
#include <villas/format_type.h>

static pthread_t re_pthread;

static int rtp_set_rate(struct node *n, double rate)
{
	struct rtp *r = (struct rtp *) n->_vd;

	switch (r->rtcp.throttle_mode) {
		case RTCP_THROTTLE_HOOK_LIMIT_RATE:
			limit_rate_set_rate(r->rtcp.throttle_hook, rate);

			break;

		case RTCP_THROTTLE_HOOK_DECIMATE:
			decimate_set_ratio(r->rtcp.throttle_hook, r->rate / rate);
			break;

		default:
			return -1;
	}

	return 0;
}

static int rtp_aimd(struct node *n, double loss_frac)
{
	struct rtp *r = (struct rtp *) n->_vd;

	int ret;
	double rate;

	if (loss_frac < 1e-3)
		rate = r->aimd.last_rate + r->aimd.a;
	else
		rate = r->aimd.last_rate * r->aimd.b;

	r->aimd.last_rate = rate;

	ret = rtp_set_rate(n, rate);
	if (ret)
		return ret;

	return 0;
}

int rtp_reverse(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;
	struct sa tmp;

	tmp = r->local_rtp;
	r->local_rtp = r->remote_rtp;
	r->remote_rtp = tmp;

	tmp = r->local_rtcp;
	r->local_rtcp = r->remote_rtcp;
	r->remote_rtcp = tmp;

	return 0;
}

int rtp_parse(struct node *n, json_t *cfg)
{
	int ret = 0;
	struct rtp *r = (struct rtp *) n->_vd;

	const char *local, *remote;
	const char *format = "villas.binary";
	uint16_t port;

	json_error_t err;
	json_t *json_rtcp = NULL, *json_aimd = NULL;

	/* Default values */
	r->aimd.a = 10;
	r->aimd.b = 0.5;
	r->aimd.last_rate = 1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: o, s?: o, s: { s: s }, s: { s: s } }",
		"format", &format,
		"rate", &r->rate,
		"rtcp", &json_rtcp,
		"aimd", &json_aimd,
		"out",
			"address", &remote,
		"in",
			"address", &local
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	/* AIMD */
	if (json_aimd) {
		ret = json_unpack_ex(json_rtcp, &err, 0, "{ s?: f, s?: f }",
			"a", &r->aimd.a,
			"b", &r->aimd.b
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));
	}

	/* RTCP */
	if (json_rtcp) {
		const char *mode = "aimd";
		const char *throttle_mode = "decimate";

		ret = json_unpack_ex(json_rtcp, &err, 0, "{ s?: b, s?: s }",
			"enabled", &r->rtcp.enabled,
			"mode", &mode,
			"throttle_mode", &throttle_mode
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		/* RTCP Mode */
		if (!strcmp(mode, "aimd"))
			r->rtcp.mode = RTCP_MODE_AIMD;
		else
			error("Unknown RTCP mode: %s", mode);

		/* RTCP Throttle mode */
		if (!strcmp(throttle_mode, "decimate"))
			r->rtcp.throttle_mode = RTCP_THROTTLE_HOOK_DECIMATE;
		else if (!strcmp(throttle_mode, "limit_rate"))
			r->rtcp.throttle_mode = RTCP_THROTTLE_HOOK_LIMIT_RATE;
		else
			error("Unknown RTCP throttle mode: %s", throttle_mode);
	}

	/* Format */
	r->format = format_type_lookup(format);
	if(!r->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	/* Remote address */
	ret = sa_decode(&r->remote_rtp, remote, strlen(remote));
	if (ret) {
		error("Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), strerror(ret));
	}

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->remote_rtp) & ~1;
	sa_set_sa(&r->remote_rtcp, &r->remote_rtp.u.sa);
	sa_set_port(&r->remote_rtp, port);
	sa_set_port(&r->remote_rtcp, port+1);

	/* Local address */
	ret = sa_decode(&r->local_rtp, local, strlen(local));
	if (ret) {
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), strerror(ret));
	}

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->local_rtp) & ~1;
	sa_set_sa(&r->local_rtcp, &r->local_rtp.u.sa);
	sa_set_port(&r->local_rtp, port);
	sa_set_port(&r->local_rtcp, port+1);

	/** @todo parse * in addresses */

	return ret;
}

char * rtp_print(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;
	char *buf;

	char *local = socket_print_addr((struct sockaddr *) &r->local_rtp.u);
	char *remote = socket_print_addr((struct sockaddr *) &r->remote_rtp.u);

	buf = strf("format=%s, in.address=%s, out.address=%s, rtcp.enabled=%s",
		format_type_name(r->format),
		local, remote,
		r->rtcp.enabled ? "yes" : "no");

	if (r->rtcp.enabled) {
		const char *mode, *throttle_mode;

		switch (r->rtcp.mode) {
			case RTCP_MODE_AIMD:
				mode = "aimd";
		}

		switch (r->rtcp.throttle_mode) {
			case RTCP_THROTTLE_HOOK_DECIMATE:
				throttle_mode = "decimate";

			case RTCP_THROTTLE_HOOK_LIMIT_RATE:
				throttle_mode = "limit_rate";
		}

		strcatf(&buf, "rtcp.mode=%s, rtcp.throttle_mode=%s", mode, throttle_mode);
	}

	free(local);
	free(remote);

	return buf;
}

static void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
	struct node *n = (struct node *) arg;
	struct rtp *r = (struct rtp *) n->_vd;

	if (queue_signalled_push(&r->recv_queue, (void *) mbuf_alloc_ref(mb)) != 1)
		warning("Failed to push to queue");

	/* source, header not yet used */
	(void) src;
	(void) hdr;
}

static void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
	struct node *n = (struct node *) arg;
	//struct rtp *r = (struct rtp *) n->_vd;

	(void)src;

	printf("rtcp: recv %s\n", rtcp_type_name(msg->hdr.pt));

	/** @todo: parse receive report */
	double loss_frac = 0;

	rtp_aimd(n, loss_frac);
}

int rtp_start(struct node *n)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	/* Initialize Queue */
	ret = queue_signalled_init(&r->recv_queue, 1024, &memory_heap, 0);
	if (ret)
		return ret;

	/* Initialize IO */
	ret = io_init(&r->io, r->format, &n->signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&r->io);
	if (ret)
		return ret;

	/* Initialize throttle hook */
	struct hook_type *throttle_hook_type;

	r->rtcp.throttle_hook = alloc(sizeof(struct hook));
	if (!r->rtcp.throttle_hook)
		return -1;

	switch (r->rtcp.throttle_mode) {
		case RTCP_THROTTLE_HOOK_DECIMATE:
			throttle_hook_type = hook_type_lookup("decimate");
			break;

		case RTCP_THROTTLE_HOOK_LIMIT_RATE:
			throttle_hook_type = hook_type_lookup("limit_rate");
			break;
	}

	if (!throttle_hook_type)
		return -1;

	ret = hook_init(r->rtcp.throttle_hook, throttle_hook_type, NULL, n);
	if (ret)
		return ret;

	vlist_push(&n->out.hooks, r->rtcp.throttle_hook);

	/* Initialize RTP socket */
	uint16_t port = sa_port(&r->local_rtp) & ~1;
	ret = rtp_listen(&r->rs, IPPROTO_UDP, &r->local_rtp, port, port+1, r->rtcp.enabled, rtp_handler, rtcp_handler, n);

	/* Start RTCP session */
	rtcp_start(r->rs, node_name(n), &r->remote_rtcp);

	return ret;
}

int rtp_stop(struct node *n)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	mem_deref(r->rs);

	ret = queue_signalled_close(&r->recv_queue);
	if (ret)
		warning("Problem closing queue");

	ret = queue_signalled_destroy(&r->recv_queue);
	if (ret)
		warning("Problem destroying queue");

	return io_destroy(&r->io);
}

static void * th_func(void *arg)
{
	re_main(NULL);
	return NULL;
}

static void stop_handler(int sig, siginfo_t *si, void *ctx)
{
	re_cancel();
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

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = stop_handler;

	ret = sigaction(SIGUSR1, &sa, NULL);
	if (ret)
		return ret;

	return ret;
}

int rtp_type_stop()
{
	int ret;

	/* Join worker thread */
	pthread_kill(re_pthread, SIGUSR1);
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
	ret = queue_signalled_pull(&r->recv_queue, (void **) &mb);
	if (ret <= 0) {
		if (ret < 0)
			warning("Failed to pull from queue");
		return ret;
	}

	/* Read from mbuf */
	bytes = mbuf_get_left(mb);
	buf = (char *) alloc(bytes);
	mbuf_read_mem(mb, (uint8_t *) buf, bytes);

	/* Unpack data */
	ret = io_sscan(&r->io, buf, bytes, NULL, smps, cnt);
	if (ret < 0)
		warning("Received invalid packet from node %s: reason=%d", node_name(n), ret);

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
		warning("Error allocating buffer space");
		return -1;
	}

retry:	cnt = io_sprint(&r->io, buf, buflen, &wbytes, smps, cnt);
	if (cnt < 0) {
		warning("Error from io_sprint, reason: %d", cnt);
		goto out1;
	}

	if (wbytes <= 0) {
		warning("Error written bytes = %ld <= 0", wbytes);
		cnt = -1;
		goto out1;
	}

	if (wbytes > buflen) {
		buflen = wbytes;
		buf = realloc(buf, buflen);
		goto retry;
	}

	/* Prepare mbuf */
	struct mbuf *mb = mbuf_alloc(buflen + 12);
	if (!mb) {
		warning("Failed to allocate memory");
		cnt = -1;
		goto out2;
	}

	ret = mbuf_write_str(mb, pad);
	if (ret) {
		warning("Error writing padding to mbuf");
		cnt = ret;
		goto out2;
	}

	ret = mbuf_write_mem(mb, (uint8_t*)buf, buflen);
	if (ret) {
		warning("Error writing data to mbuf");
		cnt = ret;
		goto out2;
	}

	mbuf_set_pos(mb, 12);

	/* Send dataset */
	ret = rtp_send(r->rs, &r->remote_rtp, false, false, 21, (uint32_t) time(NULL), mb);
	if (ret) {
		warning("Error from rtp_send, reason: %d", ret);
		cnt = ret;
	}

out2:	mem_deref(mb);
out1:	free(buf);

	return cnt;
}

static struct plugin p = {
	.name		= "rtp",
	.description	= "real-time transport protocol (libre)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct rtp),
		.type.start	= rtp_type_start,
		.type.stop	= rtp_type_stop,
		.reverse	= rtp_reverse,
		.parse		= rtp_parse,
		.print		= rtp_print,
		.start		= rtp_start,
		.stop		= rtp_stop,
		.read		= rtp_read,
		.write		= rtp_write,
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
