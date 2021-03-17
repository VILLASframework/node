/** Utilities.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <vector>
#include <string>
#include <iostream>

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <unistd.h>
#include <cmath>
#include <pthread.h>
#include <cctype>
#include <fcntl.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include <villas/config.h>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>

static pthread_t main_thread;

namespace villas {
namespace utils {

std::vector<std::string> tokenize(std::string s, std::string delimiter)
{
	std::vector<std::string> tokens;

	size_t lastPos = 0;
	size_t curentPos;

	while ((curentPos = s.find(delimiter, lastPos)) != std::string::npos) {
		const size_t tokenLength = curentPos - lastPos;
		tokens.push_back(s.substr(lastPos, tokenLength));

		/* Advance in string */
		lastPos = curentPos + delimiter.length();
	}

	/* Check if there's a last token behind the last delimiter. */
	if (lastPos != s.length()) {
		const size_t lastTokenLength = s.length() - lastPos;
		tokens.push_back(s.substr(lastPos, lastTokenLength));
	}

	return tokens;
}

ssize_t read_random(char *buf, size_t len)
{
	int fd;
	ssize_t bytes = -1;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;

	while (len) {
		bytes = read(fd, buf, len);
		if (bytes < 0)
			break;

		len -= bytes;
		buf += bytes;
	}

	close(fd);

	return bytes;
}

/* Setup exit handler */
int signals_init(void (*cb)(int signal, siginfo_t *sinfo, void *ctx), std::list<int> cbSignals, std::list<int> ignoreSignals)
{
	int ret;

	Logger logger = logging.get("signals");

	logger->info("Initialize subsystem");

	struct sigaction sa_cb;
	sa_cb.sa_flags = SA_SIGINFO | SA_NODEFER;
	sa_cb.sa_sigaction = cb;

	struct sigaction sa_ign;
	sa_ign.sa_flags = 0;
	sa_ign.sa_handler = SIG_IGN;

	main_thread = pthread_self();

	sigemptyset(&sa_cb.sa_mask);
	sigemptyset(&sa_ign.sa_mask);

	cbSignals.insert(cbSignals.begin(), { SIGINT, SIGTERM, SIGUSR1, SIGALRM });
	cbSignals.sort();
	cbSignals.unique();

	for (auto signal : cbSignals) {
		ret = sigaction(signal, &sa_cb, nullptr);
		if (ret)
			return ret;
	}

	for (auto signal : ignoreSignals) {
		ret = sigaction(signal, &sa_ign, nullptr);
		if (ret)
			return ret;
	}

	return 0;
}

char * decolor(char *str)
{
	char *p, *q;
	bool inseq = false;

	for (p = q = str; *p; p++) {
		switch (*p) {
			case 0x1b:
				if (*(++p) == '[') {
					inseq = true;
					continue;
				}
				break;

			case 'm':
				if (inseq) {
					inseq = false;
					continue;
				}
				break;
		}

		if (!inseq) {
			*q = *p;
			q++;
		}
	}

	*q = '\0';

	return str;
}

void killme(int sig)
{
	/* Send only to main thread in case the ID was initilized by signals_init() */
	if (main_thread)
		pthread_kill(main_thread, sig);
	else
		kill(0, sig);
}

double box_muller(float m, float s)
{
	double x1, x2, y1;
	static double y2;
	static int use_last = 0;

	if (use_last) {		/* use value from previous call */
		y1 = y2;
		use_last = 0;
	}
	else {
		double w;
		do {
			x1 = 2.0 * randf() - 1.0;
			x2 = 2.0 * randf() - 1.0;
			w = x1*x1 + x2*x2;
		} while (w >= 1.0);

		w = sqrt(-2.0 * log(w) / w);
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}

	return m + y1 * s;
}

double randf()
{
	return (double) random() / RAND_MAX;
}

char * vstrcatf(char **dest, const char *fmt, va_list ap)
{
	char *tmp;
	int n = *dest ? strlen(*dest) : 0;
	int i = vasprintf(&tmp, fmt, ap);

	*dest = (char *)(realloc(*dest, n + i + 1));
	if (*dest != nullptr)
		strncpy(*dest+n, tmp, i + 1);

	free(tmp);

	return *dest;
}

char * strcatf(char **dest, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vstrcatf(dest, fmt, ap);
	va_end(ap);

	return *dest;
}

char * strf(const char *fmt, ...)
{
	char *buf = nullptr;

	va_list ap;
	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	return buf;
}

char * vstrf(const char *fmt, va_list va)
{
	char *buf = nullptr;

	vstrcatf(&buf, fmt, va);

	return buf;
}

void * memdup(const void *src, size_t bytes)
{
	void *dst = new char[bytes];
	if (!dst)
		throw MemoryAllocationError();

	memcpy(dst, src, bytes);

	return dst;
}

pid_t spawn(const char* name, char *const argv[])
{
	pid_t pid;

	pid = fork();
	switch (pid) {
		case -1: return -1;
		case 0:  return execvp(name, (char * const*) argv);
	}

	return pid;
}

size_t strlenp(const char *str)
{
	size_t sz = 0;

	for (const char *d = str; *d; d++) {
		const unsigned char *c = (const unsigned char *) d;

		if (isprint(*c))
			sz++;
		else if (c[0] == '\b')
			sz--;
		else if (c[0] == '\t')
			sz += 4; /* tab width == 4 */
		/* CSI sequence */
		else if (c[0] == '\e' && c[1] == '[') {
			c += 2;
			while (*c && *c != 'm')
				c++;
		}
		/* UTF-8 */
		else if (c[0] >= 0xc2 && c[0] <= 0xdf) {
			sz++;
			c += 1;
		}
		else if (c[0] >= 0xe0 && c[0] <= 0xef) {
			sz++;
			c += 2;
		}
		else if (c[0] >= 0xf0 && c[0] <= 0xf4) {
			sz++;
			c += 3;
		}

		d = (const char *) c;
	}

	return sz;
}

int log2i(long long x) {
	if (x == 0)
		return 1;

	return sizeof(x) * 8 - __builtin_clzll(x) - 1;
}

int sha1sum(FILE *f, unsigned char *sha1)
{
	SHA_CTX c;
	char buf[512];
	ssize_t bytes;
	long seek;

	seek = ftell(f);
	fseek(f, 0, SEEK_SET);

	SHA1_Init(&c);

	bytes = fread(buf, 1, 512, f);
	while (bytes > 0) {
		SHA1_Update(&c, buf, bytes);
		bytes = fread(buf, 1, 512, f);
	}

	SHA1_Final(sha1, &c);

	fseek(f, seek, SEEK_SET);

	return 0;
}

int uuid_generate_from_str(uuid_t out, const std::string &data, const std::string &ns)
{
	int ret;
	MD5_CTX c;

	ret = MD5_Init(&c);
	if (!ret)
		return -1;

	/* Namespace */
	ret = MD5_Update(&c, (unsigned char *) ns.c_str(), ns.size());
	if (!ret)
		return -1;

	/* Data */
	ret = MD5_Update(&c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = MD5_Final((unsigned char *) out, &c);
	if (!ret)
		return -1;

	return 0;
}

int uuid_generate_from_str(uuid_t out, const std::string &data, const uuid_t ns)
{
	int ret;
	MD5_CTX c;

	ret = MD5_Init(&c);
	if (!ret)
		return -1;

	/* Namespace */
	ret = MD5_Update(&c, (unsigned char *) ns, 16);
	if (!ret)
		return -1;

	/* Data */
	ret = MD5_Update(&c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = MD5_Final((unsigned char *) out, &c);
	if (!ret)
		return -1;

	return 0;
}

int uuid_generate_from_json(uuid_t out, json_t *json, const uuid_t ns)
{
	char *str = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);

	int ret = uuid_generate_from_str(out, str, ns);

	free(str);

	return ret;
}

bool is_docker()
{
	return access("/.dockerenv", F_OK) != -1;
}

bool is_kubernetes()
{
	return access("/var/run/secrets/kubernetes.io", F_OK) != -1 ||
	       getenv("KUBERNETES_SERVICE_HOST") != nullptr;
}

bool is_container() {
	return is_docker() || is_kubernetes();
}

} /* namespace utils */
} /* namespace villas */
