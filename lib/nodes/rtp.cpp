/** Node type: Real-time Protocol (RTP)
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Marvin Klimke <marvin.klimke@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <cinttypes>
#include <pthread.h>
#include <cstring>
#include <ctime>
#include <signal.h>

#include <villas/nodes/rtp.hpp>

extern "C" {
	#include <re/re_main.h>
	#include <re/re_types.h>
	#include <re/re_mbuf.h>
	#include <re/re_mem.h>
	#include <re/re_sys.h>
	#include <re/re_udp.h>
	#undef ALIGN_MASK
}

#include <villas/node_compat.hpp>
#include <villas/nodes/socket.hpp>
#include <villas/utils.hpp>
#include <villas/stats.hpp>
#include <villas/hook.hpp>
#include <villas/super_node.hpp>

#ifdef WITH_NETEM
  #include <villas/kernel/if.hpp>
#endif /* WITH_NETEM */

static pthread_t re_pthread;

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::kernel;

static NodeCompatType p;
static NodeCompatFactory ncp(&p);

static
int rtp_aimd(NodeCompat *n, double loss_frac)
{
	auto *r = n->getData<struct rtp>();

	double rate;

	if (!r->rtcp.enabled)
		return -1;

	if (loss_frac < 0.01)
		rate = r->aimd.rate + r->aimd.a;
	else
		rate = r->aimd.rate * r->aimd.b;

	r->aimd.rate = r->aimd.rate_pid.calculate(rate, r->aimd.rate);

	if (r->aimd.rate_hook) {
		r->aimd.rate_hook->setRate(r->aimd.rate);
		n->logger->debug("AIMD: Set rate limit to: {}", r->aimd.rate);
	}

	if (r->aimd.log)
		*(r->aimd.log) << r->rtcp.num_rrs << "\t" << loss_frac << "\t" << r->aimd.rate << std::endl;

	n->logger->debug("AIMD: {}\t{}\t{}", r->rtcp.num_rrs, loss_frac, r->aimd.rate);

	return 0;
}

int villas::node::rtp_init(NodeCompat *n)
{
	auto *r = n->getData<struct rtp>();

	n->logger = villas::logging.get("node:rtp");

	/* Default values */
	r->aimd.a = 10;
	r->aimd.b = 0.5;
	r->aimd.Kp = 1;
	r->aimd.Ki = 0;
	r->aimd.Kd = 0;

	r->aimd.rate = 1;
	r->aimd.rate_min = 1;
	r->aimd.rate_source = 2000;

	r->aimd.log_filename = nullptr;
	r->aimd.log = nullptr;

	r->rtcp.enabled = false;
	r->aimd.rate_hook_type = RTPHookType::DISABLED;

	r->formatter = nullptr;

	return 0;
}

int villas::node::rtp_reverse(NodeCompat *n)
{
	auto *r = n->getData<struct rtp>();

	SWAP(r->in.saddr_rtp, r->out.saddr_rtp);
	SWAP(r->in.saddr_rtcp, r->out.saddr_rtcp);

	return 0;
}

int villas::node::rtp_parse(NodeCompat *n, json_t *json)
{
	int ret = 0;
	auto *r = n->getData<struct rtp>();

	const char *local, *remote;
	const char *log = nullptr;
	const char *hook_type = nullptr;
	uint16_t port;

	json_error_t err;
	json_t *json_aimd = nullptr;
	json_t *json_format = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: b, s?: o, s: { s: s }, s: { s: s } }",
		"format", &json_format,
		"rtcp", &r->rtcp.enabled,
		"aimd", &json_aimd,
		"out",
			"address", &remote,
		"in",
			"address", &local
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-rtp");

	/* AIMD */
	if (json_aimd) {
		ret = json_unpack_ex(json_aimd, &err, 0, "{ s?: F, s?: F, s?: F, s?: F, s?: F, s?: F, s?: F, s?: F, s?: s, s?: s }",
			"a", &r->aimd.a,
			"b", &r->aimd.b,
			"Kp", &r->aimd.Kp,
			"Ki", &r->aimd.Ki,
			"Kd", &r->aimd.Kd,
			"rate_min", &r->aimd.rate_min,
			"rate_source", &r->aimd.rate_source,
			"rate_init", &r->aimd.rate,
			"log", &log,
			"hook_type", &hook_type
		);
		if (ret)
			throw ConfigError(json_aimd, err, "node-config-node-rtp-aimd");

		/* AIMD Hook type */
		if (!r->rtcp.enabled)
			r->aimd.rate_hook_type = RTPHookType::DISABLED;
		else if (hook_type) {
			if (!strcmp(hook_type, "decimate"))
				r->aimd.rate_hook_type = RTPHookType::DECIMATE;
			else if (!strcmp(hook_type, "limit_rate"))
				r->aimd.rate_hook_type = RTPHookType::LIMIT_RATE;
			else if (!strcmp(hook_type, "disabled"))
				r->aimd.rate_hook_type = RTPHookType::DISABLED;
			else
				throw RuntimeError("Unknown RTCP hook_type: {}", hook_type);
		}
	}

	if (log)
		r->aimd.log_filename = strdup(log);

	/* Format */
	if (r->formatter)
		delete r->formatter;
	r->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.binary");
	if (!r->formatter)
		throw ConfigError(json_format, "node-config-node-rtp-format", "Invalid format configuration");

	/* Remote address */
	ret = sa_decode(&r->out.saddr_rtp, remote, strlen(remote));
	if (ret)
		throw RuntimeError("Failed to resolve remote address '{}': {}", remote, strerror(ret));

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->out.saddr_rtp) & ~1;
	sa_set_sa(&r->out.saddr_rtcp, &r->out.saddr_rtp.u.sa);
	sa_set_port(&r->out.saddr_rtp, port);
	sa_set_port(&r->out.saddr_rtcp, port+1);

	/* Local address */
	ret = sa_decode(&r->in.saddr_rtp, local, strlen(local));
	if (ret)
		throw RuntimeError("Failed to resolve local address '{}': {}", local, strerror(ret));

	/* Assign even port number to RTP socket, next odd number to RTCP socket */
	port = sa_port(&r->in.saddr_rtp) & ~1;
	sa_set_sa(&r->in.saddr_rtcp, &r->in.saddr_rtp.u.sa);
	sa_set_port(&r->in.saddr_rtp, port);
	sa_set_port(&r->in.saddr_rtcp, port+1);

	/** @todo parse * in addresses */

	return 0;
}

char * villas::node::rtp_print(NodeCompat *n)
{
	auto *r = n->getData<struct rtp>();
	char *buf;

	char *local = socket_print_addr((struct sockaddr *) &r->in.saddr_rtp.u);
	char *remote = socket_print_addr((struct sockaddr *) &r->out.saddr_rtp.u);

	buf = strf("in.address=%s, out.address=%s, rtcp.enabled=%s",
		local, remote,
		r->rtcp.enabled ? "yes" : "no");

	if (r->rtcp.enabled) {
		const char *hook_type;

		switch (r->aimd.rate_hook_type) {
			case RTPHookType::DECIMATE:
				hook_type = "decimate";
				break;

			case RTPHookType::LIMIT_RATE:
				hook_type = "limit_rate";
				break;

			case RTPHookType::DISABLED:
				hook_type = "disabled";
				break;

			default:
				hook_type = "unknown";
		}

		strcatf(&buf, ", aimd.hook_type=%s", hook_type);
		strcatf(&buf, ", aimd.a=%f, aimd.b=%f, aimd.start_rate=%f", r->aimd.a, r->aimd.b, r->aimd.rate);
	}

	free(local);
	free(remote);

	return buf;
}

static
void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
	int ret;
	auto *n = (NodeCompat *) arg;
	auto *r = n->getData<struct rtp>();

	/* source, header not used */
	(void) src;
	(void) hdr;

	void *d = mem_ref((void *) mb);

	ret = queue_signalled_push(&r->recv_queue, d);
	if (ret != 1) {
		n->logger->warn("Failed to push to queue");
		mem_deref(d);
	}
}

static
void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
	auto *n = (NodeCompat *) arg;
	auto *r = n->getData<struct rtp>();

	/* source not used */
	(void) src;

	n->logger->debug("RTCP: recv {}", rtcp_type_name((enum rtcp_type) msg->hdr.pt));

	if (msg->hdr.pt == RTCP_SR) {
		if (msg->hdr.count > 0) {
			const struct rtcp_rr *rr = &msg->r.sr.rrv[0];

			double loss_frac = (double) rr->fraction / 256;

			rtp_aimd(n, loss_frac);

			auto stats = n->getStats();
			if (stats) {
				stats->update(Stats::Metric::RTP_PKTS_LOST, rr->lost);
				stats->update(Stats::Metric::RTP_LOSS_FRACTION, loss_frac);
				stats->update(Stats::Metric::RTP_JITTER, rr->jitter);
			}

			n->logger->info("RTCP: rr: num_rrs={}, loss_frac={}, pkts_lost={}, jitter={}", r->rtcp.num_rrs, loss_frac, rr->lost, rr->jitter);
		}
		else
			n->logger->debug("RTCP: Received sender report with zero reception reports");
	}

	r->rtcp.num_rrs++;
}

int villas::node::rtp_start(NodeCompat *n)
{
	int ret;
	auto *r = n->getData<struct rtp>();

	/* Initialize queue */
	ret = queue_signalled_init(&r->recv_queue, 1024, &memory::heap);
	if (ret)
		return ret;

	/* Initialize IO */
	r->formatter->start(n->getInputSignals(false), ~(int) SampleFlags::HAS_OFFSET);

	/* Initialize memory buffer for sending */
	r->send_mb = mbuf_alloc(RTP_INITIAL_BUFFER_LEN);
	if (!r->send_mb)
		return -1;

	ret = mbuf_fill(r->send_mb, 0, RTP_HEADER_SIZE);
	if (ret)
		return -1;

	/* Initialize AIMD hook */
	if (r->aimd.rate_hook_type != RTPHookType::DISABLED) {
#ifdef WITH_HOOKS
		switch (r->aimd.rate_hook_type) {
			case RTPHookType::DECIMATE:
				r->aimd.rate_hook = std::make_shared<DecimateHook>(nullptr, n, 0, 0);
				break;

			case RTPHookType::LIMIT_RATE:
				r->aimd.rate_hook = std::make_shared<LimitRateHook>(nullptr, n, 0, 0);
				break;

			default:
				return -1;
		}

		if (!r->aimd.rate_hook)
			throw MemoryAllocationError();

		r->aimd.rate_hook->init();

		n->out.hooks.push_back(r->aimd.rate_hook);

		r->aimd.rate_hook->setRate(r->aimd.rate, r->aimd.rate_source);
#else
		throw RuntimeError("Rate limiting is not supported");

		return -1;
#endif
	}

	double dt = 5.0; // TODO

	r->aimd.rate_pid = villas::dsp::PID(dt, r->aimd.rate_source, r->aimd.rate_min, r->aimd.Kp, r->aimd.Ki, r->aimd.Kd);

	/* Initialize RTP socket */
	uint16_t port = sa_port(&r->in.saddr_rtp) & ~1;
	ret = rtp_listen(&r->rs, IPPROTO_UDP, &r->in.saddr_rtp, port, port+1, r->rtcp.enabled, rtp_handler, rtcp_handler, n);

	/* Start RTCP session */
	if (r->rtcp.enabled) {
		r->rtcp.num_rrs = 0;

		rtcp_start(r->rs, n->getNameShort().c_str(), &r->out.saddr_rtcp);

		if (r->aimd.log_filename) {
			char fn[128];

			time_t ts = time(nullptr);
			struct tm tm;

			/* Convert time */
			gmtime_r(&ts, &tm);
			strftime(fn, sizeof(fn), r->aimd.log_filename, &tm);

			r->aimd.log = new std::ofstream(fn, std::ios::out | std::ios::trunc);
			if (!r->aimd.log)
				throw MemoryAllocationError();

			*(r->aimd.log) << "# cnt\tfrac_loss\trate" << std::endl;
		}
		else
			r->aimd.log = nullptr;
	}

	return ret;
}

int villas::node::rtp_stop(NodeCompat *n)
{
	int ret;
	auto *r = n->getData<struct rtp>();

	mem_deref(r->rs);

	ret = queue_signalled_close(&r->recv_queue);
	if (ret)
		throw RuntimeError("Problem closing queue");

	ret = queue_signalled_destroy(&r->recv_queue);
	if (ret)
		throw RuntimeError("Problem destroying queue");

	mem_deref(r->send_mb);

	if (r->aimd.log)
		r->aimd.log->close();

	return 0;
}

int villas::node::rtp_destroy(NodeCompat *n)
{
	auto *r = n->getData<struct rtp>();

	if (r->aimd.log)
		delete r->aimd.log;

	if (r->aimd.log_filename)
		free(r->aimd.log_filename);

	if (r->formatter)
		delete r->formatter;

	return 0;
}

static
void stop_handler(int sig, siginfo_t *si, void *ctx)
{
	re_cancel();
}

typedef void *(*pthread_start_routine)(void *);

int villas::node::rtp_type_start(villas::node::SuperNode *sn)
{
	int ret;

	/* Initialize library */
	ret = libre_init();
	if (ret)
		throw RuntimeError("Error initializing libre");

	/* Add worker thread */
	ret = pthread_create(&re_pthread, nullptr, (pthread_start_routine) re_main, nullptr);
	if (ret)
		throw RuntimeError("Error creating rtp node type pthread");

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = stop_handler;

	ret = sigaction(SIGUSR1, &sa, nullptr);
	if (ret)
		return ret;

#ifdef WITH_NETEM
	/* Gather list of used network interfaces */
	for (auto *n : ncp.instances) {
		auto *nc = dynamic_cast<NodeCompat *>(n);
		auto *r = nc->getData<struct rtp>();
		Interface *j = Interface::getEgress(&r->out.saddr_rtp.u.sa, sn);

		if (!j)
			throw RuntimeError("Failed to find egress interface");

		j->addNode(n);
	}
#endif /* WITH_NETEM */

	return 0;
}

int villas::node::rtp_type_stop()
{
	int ret;

	/* Join worker thread */
	pthread_kill(re_pthread, SIGUSR1);
	ret = pthread_join(re_pthread, nullptr);
	if (ret)
		throw RuntimeError("Error joining rtp node type pthread");

	libre_close();

	return 0;
}

int villas::node::rtp_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *r = n->getData<struct rtp>();
	struct mbuf *mb;

	/* Get data from queue */
	ret = queue_signalled_pull(&r->recv_queue, (void **) &mb);
	if (ret < 0)
		throw RuntimeError("Failed to pull from queue");

	/* Unpack data */
	ret = r->formatter->sscan((char *) mb->buf + mb->pos, mbuf_get_left(mb), nullptr, smps, cnt);

	mem_deref(mb);

	return ret;
}

int villas::node::rtp_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *r = n->getData<struct rtp>();

	size_t wbytes;
	size_t avail;

	uint32_t ts = (uint32_t) time(nullptr);

retry:	mbuf_set_pos(r->send_mb, RTP_HEADER_SIZE);
	avail = mbuf_get_space(r->send_mb);
	cnt = r->formatter->sprint((char *) r->send_mb->buf + r->send_mb->pos, avail, &wbytes, smps, cnt);
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
	if (ret)
		throw RuntimeError("Error from rtp_send, reason: {}", ret);

	return cnt;
}

int villas::node::rtp_poll_fds(NodeCompat *n, int fds[])
{
	auto *r = n->getData<struct rtp>();

	fds[0] = queue_signalled_fd(&r->recv_queue);

	return 1;
}

int villas::node::rtp_netem_fds(NodeCompat *n, int fds[])
{
	auto *r = n->getData<struct rtp>();

	int m = 0;
	struct udp_sock *rtp = (struct udp_sock *) rtp_sock(r->rs);
	struct udp_sock *rtcp = (struct udp_sock *) rtcp_sock(r->rs);

	fds[m++] = udp_sock_fd(rtp, AF_INET);

	if (r->rtcp.enabled)
		fds[m++] = udp_sock_fd(rtcp, AF_INET);

	return m;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "rtp";
#ifdef WITH_NETEM
	p.description		= "real-time transport protocol (libre, libnl3 netem support)";
#else
	p.description		= "real-time transport protocol (libre)";
#endif
	p.vectorize	= 0;
	p.size		= sizeof(struct rtp);
	p.type.start	= rtp_type_start;
	p.type.stop	= rtp_type_stop;
	p.init		= rtp_init;
	p.destroy	= rtp_destroy;
	p.parse		= rtp_parse;
	p.print		= rtp_print;
	p.start		= rtp_start;
	p.stop		= rtp_stop;
	p.read		= rtp_read;
	p.write		= rtp_write;
	p.reverse	= rtp_reverse;
	p.poll_fds	= rtp_poll_fds;
	p.netem_fds	= rtp_netem_fds;
}
