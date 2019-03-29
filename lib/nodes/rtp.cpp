/** Node type: Real-time Protocol (RTP)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
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

extern "C" {
#include <re/re_types.h>
#include <re/re_main.h>
#include <re/re_mbuf.h>
#include <re/re_mem.h>
#include <re/re_rtp.h>
#include <re/re_sys.h>
#include <re/re_udp.h>
#undef ALIGN_MASK
}

#include <villas/plugin.h>
#include <villas/nodes/socket.h>
#include <villas/nodes/rtp.hpp>
#include <villas/utils.h>
#include <villas/stats.h>
#include <villas/hook.h>
#include <villas/format_type.h>
#include <villas/super_node.h>

#ifdef WITH_NETEM
  #include <villas/kernel/if.h>
#endif /* WITH_NETEM */

static pthread_t re_pthread;

using namespace villas::node;

extern "C" {

/* Forward declarations */
extern struct plugin p;

static int rtp_set_rate(struct node *n, double rate)
{
	struct rtp *r = (struct rtp *) n->_vd;
	int ratio;

	switch (r->rtcp.throttle_mode) {
		case RTCP_THROTTLE_HOOK_LIMIT_RATE:
			r->rtcp.throttle_hook.limit_rate->setRate(rate);
			break;

		case RTCP_THROTTLE_HOOK_DECIMATE:
			ratio = r->rate / rate;
			if (ratio == 0)
				ratio = 1;

			r->rtcp.throttle_hook.decimate->setRatio(ratio);
			break;

		case RTCP_THROTTLE_DISABLED:
			return 0;

		default:
			return -1;
	}

	r->logger->debug("Set rate limiting for node {} to {}", node_name(n), rate);

	return 0;
}

static int rtp_aimd(struct node *n, double loss_frac)
{
	struct rtp *r = (struct rtp *) n->_vd;

	int ret;
	double rate;

	if (!r->rtcp.enabled)
		return -1;

	if (loss_frac < 0.01)
		rate = r->aimd.last_rate + r->aimd.a;
	else
		rate = r->aimd.last_rate * r->aimd.b;

	r->aimd.last_rate = rate;

	ret = rtp_set_rate(n, rate);
	if (ret)
		return ret;

	if (r->aimd.log)
		*(r->aimd.log) << r->rtcp.num_rrs << "\t" << loss_frac << "\t" << rate << std::endl;

	r->logger->debug("AIMD: {}\t{}\t{}", r->rtcp.num_rrs, loss_frac, rate);

	return 0;
}

int rtp_init(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;

	r->logger = villas::logging.get("node:rtp");

	/* Default values */
	r->rate = 1;

	r->aimd.a = 10;
	r->aimd.b = 0.5;
	r->aimd.last_rate = 2000;
	r->aimd.log_filename = nullptr;
	r->aimd.log = nullptr;

	r->rtcp.enabled = false;
	r->rtcp.throttle_mode = RTCP_THROTTLE_DISABLED;

	return 0;
}

int rtp_reverse(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;

	SWAP(r->in.saddr_rtp, r->out.saddr_rtp);
	SWAP(r->in.saddr_rtcp, r->out.saddr_rtcp);

	return 0;
}

int rtp_parse(struct node *n, json_t *cfg)
{
	int ret = 0;
	struct rtp *r = (struct rtp *) n->_vd;

	const char *local, *remote;
	const char *format = "villas.binary";
	const char *log = nullptr;
	uint16_t port;

	json_error_t err;
	json_t *json_rtcp = nullptr, *json_aimd = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: F, s?: o, s?: o, s: { s: s }, s: { s: s } }",
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
		r->logger->error("Failed to parse configuration of node {}", node_name(n));

	/* AIMD */
	if (json_aimd) {
		ret = json_unpack_ex(json_aimd, &err, 0, "{ s?: F, s?: F, s?: F, s?: s }",
			"a", &r->aimd.a,
			"b", &r->aimd.b,
			"start_rate", &r->aimd.last_rate,
			"log", &log
		);
		if (ret)
			r->logger->error("Failed to parse configuration of node {}", node_name(n));
	}

	if (log)
		r->aimd.log_filename = strdup(log);

	/* RTCP */
	if (json_rtcp) {
		const char *mode = "aimd";
		const char *throttle_mode = "decimate";

		/* Enable if RTCP section is available */
		r->rtcp.enabled = 1;

		ret = json_unpack_ex(json_rtcp, &err, 0, "{ s?: b, s?: s, s?: s }",
			"enabled", &r->rtcp.enabled,
			"mode", &mode,
			"throttle_mode", &throttle_mode
		);
		if (ret)
			r->logger->error("Failed to parse configuration of node {}", node_name(n));

		/* RTCP Mode */
		if (!strcmp(mode, "aimd"))
			r->rtcp.mode = RTCP_MODE_AIMD;
		else
			r->logger->error("Unknown RTCP mode: {}", mode);

		/* RTCP Throttle mode */
		if (r->rtcp.enabled == false)
			r->rtcp.throttle_mode = RTCP_THROTTLE_DISABLED;
		else if (!strcmp(throttle_mode, "decimate"))
			r->rtcp.throttle_mode = RTCP_THROTTLE_HOOK_DECIMATE;
		else if (!strcmp(throttle_mode, "limit_rate"))
			r->rtcp.throttle_mode = RTCP_THROTTLE_HOOK_LIMIT_RATE;
		else
			r->logger->error("Unknown RTCP throttle mode: {}", throttle_mode);
	}

	/* Format */
	r->format = format_type_lookup(format);
	if (!r->format)
		r->logger->error("Invalid format '{}' for node {}", format, node_name(n));

	/* Remote address */
	ret = sa_decode(&r->out.saddr_rtp, remote, strlen(remote));
	if (ret) {
		r->logger->error("Failed to resolve remote address '{}' of node {}: {}",
			remote, node_name(n), strerror(ret));
	}

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->out.saddr_rtp) & ~1;
	sa_set_sa(&r->out.saddr_rtcp, &r->out.saddr_rtp.u.sa);
	sa_set_port(&r->out.saddr_rtp, port);
	sa_set_port(&r->out.saddr_rtcp, port+1);

	/* Local address */
	ret = sa_decode(&r->in.saddr_rtp, local, strlen(local));
	if (ret) {
		r->logger->error("Failed to resolve local address '{}' of node {}: {}",
			local, node_name(n), strerror(ret));
	}

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->in.saddr_rtp) & ~1;
	sa_set_sa(&r->in.saddr_rtcp, &r->in.saddr_rtp.u.sa);
	sa_set_port(&r->in.saddr_rtp, port);
	sa_set_port(&r->in.saddr_rtcp, port+1);

	/** @todo parse * in addresses */

	return ret;
}

char * rtp_print(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;
	char *buf;

	char *local = socket_print_addr((struct sockaddr *) &r->in.saddr_rtp.u);
	char *remote = socket_print_addr((struct sockaddr *) &r->out.saddr_rtp.u);

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
				break;

			case RTCP_THROTTLE_HOOK_LIMIT_RATE:
				throttle_mode = "limit_rate";
				break;

			case RTCP_THROTTLE_DISABLED:
				throttle_mode = "disabled";
				break;

			default:
				throttle_mode = "unknown";
		}

		strcatf(&buf, ", rtcp.mode=%s, rtcp.throttle_mode=%s", mode, throttle_mode);

		if (r->rtcp.mode == RTCP_MODE_AIMD)
			strcatf(&buf, ", aimd.a=%f, aimd.b=%f, aimd.start_rate=%f", r->aimd.a, r->aimd.b, r->aimd.last_rate);
	}

	free(local);
	free(remote);

	return buf;
}

static void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
	int ret;
	struct node *n = (struct node *) arg;
	struct rtp *r = (struct rtp *) n->_vd;

	/* source, header not used */
	(void) src;
	(void) hdr;

	void *d = mem_ref((void *) mb);

	ret = queue_signalled_push(&r->recv_queue, d);
	if (ret != 1) {
		r->logger->warn("Failed to push to queue");
		mem_deref(d);
	}
}

static void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
	struct node *n = (struct node *) arg;
	struct rtp *r = (struct rtp *) n->_vd;

	/* source not used */
	(void) src;

	debug(5, "RTCP: recv %s", rtcp_type_name((enum rtcp_type) msg->hdr.pt));

	if (msg->hdr.pt == RTCP_SR) {
		if (msg->hdr.count > 0) {
			const struct rtcp_rr *rr = &msg->r.sr.rrv[0];
			debug(5, "RTP: fraction lost = %d", rr->fraction);

			double loss = (double) rr->fraction / 256;

			rtp_aimd(n, loss);

			if (n->stats) {
				stats_update(n->stats, STATS_METRIC_RTP_PKTS_LOST, rr->lost);
				stats_update(n->stats, STATS_METRIC_RTP_LOSS_FRACTION, loss);
				stats_update(n->stats, STATS_METRIC_RTP_JITTER, rr->jitter);
			}
		}
		else
			debug(5, "RTCP: Received sender report with zero reception reports");
	}

	r->rtcp.num_rrs++;
}

int rtp_start(struct node *n)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	/* Initialize queue */
	ret = queue_signalled_init(&r->recv_queue, 1024, &memory_heap, 0);
	if (ret)
		return ret;

	/* Initialize IO */
	ret = io_init(&r->io, r->format, &n->in.signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	/* Initialize memory buffer for sending */
	r->send_mb = mbuf_alloc(RTP_INITIAL_BUFFER_LEN);
	if (!r->send_mb)
		return -1;

	ret = mbuf_fill(r->send_mb, 0, RTP_HEADER_SIZE);
	if (ret)
		return -1;

	ret = io_check(&r->io);
	if (ret)
		return ret;

	/* Initialize throttle hook */
	if (r->rtcp.throttle_mode != RTCP_THROTTLE_DISABLED) {
		switch (r->rtcp.throttle_mode) {
			case RTCP_THROTTLE_HOOK_DECIMATE:
				r->rtcp.throttle_hook.decimate = new DecimateHook(nullptr, n, 0, 0);
				break;

			case RTCP_THROTTLE_HOOK_LIMIT_RATE:
				r->rtcp.throttle_hook.limit_rate = new LimitRateHook(nullptr, n, 0, 0);
				break;

			default:
				return -1;
		}

		vlist_push(&n->out.hooks, (void *) r->rtcp.throttle_hook.limit_rate);
	}

	ret = rtp_set_rate(n, r->aimd.last_rate);
	if (ret)
		return ret;

	/* Initialize RTP socket */
	uint16_t port = sa_port(&r->in.saddr_rtp) & ~1;
	ret = rtp_listen(&r->rs, IPPROTO_UDP, &r->in.saddr_rtp, port, port+1, r->rtcp.enabled, rtp_handler, rtcp_handler, n);

	/* Start RTCP session */
	if (r->rtcp.enabled) {
		r->rtcp.num_rrs = 0;

		rtcp_start(r->rs, node_name(n), &r->out.saddr_rtcp);

		if (r->rtcp.mode == RTCP_MODE_AIMD) {
			char fn[128];

			time_t ts = time(nullptr);
			struct tm tm;

			/* Convert time */
			gmtime_r(&ts, &tm);
			strftime(fn, sizeof(fn), r->aimd.log_filename, &tm);

			r->aimd.log = new std::ofstream(fn, std::ios::out | std::ios::trunc);

			*(r->aimd.log) << "# cnt\tfrac_loss\trate" << std::endl;
		}
	}

	return ret;
}

int rtp_stop(struct node *n)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	mem_deref(r->rs);

	ret = queue_signalled_close(&r->recv_queue);
	if (ret) {
		r->logger->error("Problem closing queue");
		return ret;
	}

	ret = queue_signalled_destroy(&r->recv_queue);
	if (ret) {
		r->logger->error("Problem destroying queue");
		return ret;
	}

	mem_deref(r->send_mb);

	if (r->aimd.log)
		r->aimd.log->close();

	ret = io_destroy(&r->io);
	if (ret)
		return ret;

	return ret;
}

int rtp_destroy(struct node *n)
{
	struct rtp *r = (struct rtp *) n->_vd;

	//r->logger.~Logger();

	if (r->aimd.log)
		delete r->aimd.log;

	if (r->aimd.log_filename)
		free(r->aimd.log_filename);

	return 0;
}

static void stop_handler(int sig, siginfo_t *si, void *ctx)
{
	re_cancel();
}

typedef void *(*pthread_start_routine)(void *);

int rtp_type_start(struct super_node *sn)
{
	int ret;

	/* Initialize library */
	ret = libre_init();
	if (ret) {
		error("Error initializing libre");
		return ret;
	}

	/* Add worker thread */
	ret = pthread_create(&re_pthread, nullptr, (pthread_start_routine) re_main, nullptr);
	if (ret) {
		error("Error creating rtp node type pthread");
		return ret;
	}

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = stop_handler;

	ret = sigaction(SIGUSR1, &sa, nullptr);
	if (ret)
		return ret;

#ifdef WITH_NETEM
	struct vlist *interfaces = super_node_get_interfaces(sn);

	/* Gather list of used network interfaces */
	for (size_t i = 0; i < vlist_length(&p.node.instances); i++) {
		struct node *n = (struct node *) vlist_at(&p.node.instances, i);
		struct rtp *r = (struct rtp *) n->_vd;
		struct interface *j = if_get_egress(&r->out.saddr_rtp.u.sa, interfaces);

		if (!j) {
			r->logger->error("Failed to find egress interface for node: {}", node_name(n));
			return -1;
		}

		vlist_push(&j->nodes, n);
	}
#endif /* WITH_NETEM */

	return ret;
}

int rtp_type_stop()
{
	int ret;

	/* Join worker thread */
	pthread_kill(re_pthread, SIGUSR1);
	ret = pthread_join(re_pthread, nullptr);
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
	struct mbuf *mb;

	/* Get data from queue */
	ret = queue_signalled_pull(&r->recv_queue, (void **) &mb);
	if (ret < 0) {
		r->logger->error("Failed to pull from queue");
		return ret;
	}

	/* Unpack data */
	ret = io_sscan(&r->io, (char *) mb->buf + mb->pos, mbuf_get_left(mb), nullptr, smps, cnt);

	mem_deref(mb);

	return ret;
}

int rtp_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct rtp *r = (struct rtp *) n->_vd;

	size_t wbytes;
	size_t avail;

	uint32_t ts = (uint32_t) time(nullptr);

retry:	mbuf_set_pos(r->send_mb, RTP_HEADER_SIZE);
	avail = mbuf_get_space(r->send_mb);
	cnt = io_sprint(&r->io, (char *) r->send_mb->buf + r->send_mb->pos, avail, &wbytes, smps, cnt);
	if (cnt < 0)
		return -1;

	if (wbytes > avail) {
		ret = mbuf_resize(r->send_mb, wbytes + RTP_HEADER_SIZE);
		if (!ret)
			return -1;

		goto retry;
	}
	else
		mbuf_set_end(r->send_mb, r->send_mb->pos + wbytes);

	mbuf_set_pos(r->send_mb, RTP_HEADER_SIZE);

	/* Send dataset */
	ret = rtp_send(r->rs, &r->out.saddr_rtp, false, false, RTP_PACKET_TYPE, ts, r->send_mb);
	if (ret) {
		r->logger->error("Error from rtp_send, reason: {}", ret);
		return ret;
	}

	return cnt;
}

int rtp_poll_fds(struct node *n, int fds[])
{
	struct rtp *r = (struct rtp *) n->_vd;

	fds[0] = queue_signalled_fd(&r->recv_queue);

	return 1;
}

int rtp_netem_fds(struct node *n, int fds[])
{
	struct rtp *r = (struct rtp *) n->_vd;

	int m = 0;
	struct udp_sock *rtp = (struct udp_sock *) rtp_sock(r->rs);
	struct udp_sock *rtcp = (struct udp_sock *) rtcp_sock(r->rs);

	fds[m++] = udp_sock_fd(rtp, AF_INET);

	if (r->rtcp.enabled)
		fds[m++] = udp_sock_fd(rtcp, AF_INET);

	return m;
}

struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "rtp";
#ifdef WITH_NETEM
	p.description	= "real-time transport protocol (libre, libnl3 netem support)";
#else
	p.description	= "real-time transport protocol (libre)";
#endif
	p.type		= PLUGIN_TYPE_NODE;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct rtp);
	p.node.type.start	= rtp_type_start;
	p.node.type.stop	= rtp_type_stop;
	p.node.init		= rtp_init;
	p.node.destroy		= rtp_destroy;
	p.node.parse		= rtp_parse;
	p.node.print		= rtp_print;
	p.node.start		= rtp_start;
	p.node.stop		= rtp_stop;
	p.node.read		= rtp_read;
	p.node.write		= rtp_write;
	p.node.reverse		= rtp_reverse;
	p.node.poll_fds	= rtp_poll_fds;
	p.node.netem_fds	= rtp_netem_fds;

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != STATE_DESTROYED)
		vlist_remove_all(&plugins, &p);
}

} /* extern C */

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
