/** Utilities.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <fcntl.h>
#include <unistd.h>

#include <villas/utils.h>
#include <villas/utils.hpp>
#include <villas/config.h>
#include <villas/log.hpp>

namespace villas {
namespace utils {

static pthread_t main_thread;

std::vector<std::string>
tokenize(std::string s, std::string delimiter)
{
	std::vector<std::string> tokens;

	size_t lastPos = 0;
	size_t curentPos;

	while((curentPos = s.find(delimiter, lastPos)) != std::string::npos) {
		const size_t tokenLength = curentPos - lastPos;
		tokens.push_back(s.substr(lastPos, tokenLength));

		/* Advance in string */
		lastPos = curentPos + delimiter.length();
	}

	/* Check if there's a last token behind the last delimiter. */
	if(lastPos != s.length()) {
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
int signals_init(void (*cb)(int signal, siginfo_t *sinfo, void *ctx))
{
	int ret;

	Logger logger = logging.get("signals");

	logger->info("Initialize subsystem");

	struct sigaction sa_quit;
	sa_quit.sa_flags = SA_SIGINFO | SA_NODEFER;
	sa_quit.sa_sigaction = cb;

	struct sigaction sa_chld;
	sa_chld.sa_flags = 0;
	sa_chld.sa_handler = SIG_IGN;

	main_thread = pthread_self();

	sigemptyset(&sa_quit.sa_mask);

	ret = sigaction(SIGINT, &sa_quit, NULL);
	if (ret)
		return ret;

	ret = sigaction(SIGTERM, &sa_quit, NULL);
	if (ret)
		return ret;

	ret = sigaction(SIGALRM, &sa_quit, NULL);
	if (ret)
		return ret;

	ret = sigaction(SIGCHLD, &sa_chld, NULL);
	if (ret)
		return ret;

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

extern "C" {

void killme(int sig)
{
	/* Send only to main thread in case the ID was initilized by signals_init() */
	if (main_thread)
		pthread_kill(main_thread, sig);
	else
		kill(0, sig);
}

}

} /* namespace utils */
} /* namespace villas */
