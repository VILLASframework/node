/** Utilities.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

namespace villas {
namespace utils {

std::vector<std::string>
tokenize(std::string s, std::string delimiter)
{
	std::vector<std::string> tokens;

	size_t lastPos = 0;
	size_t curentPos;

	while((curentPos = s.find(delimiter, lastPos)) != std::string::npos) {
		const size_t tokenLength = curentPos - lastPos;
		tokens.push_back(s.substr(lastPos, tokenLength));

		// advance in string
		lastPos = curentPos + delimiter.length();
	}

	// check if there's a last token behind the last delimiter
	if(lastPos != s.length()) {
		const size_t lastTokenLength = s.length() - lastPos;
		tokens.push_back(s.substr(lastPos, lastTokenLength));
	}

	return tokens;
}

void print_copyright()
{
	std::cout << PROJECT_NAME " " << CLR_BLU(PROJECT_BUILD_ID)
	          << " (built on " CLR_MAG(__DATE__) " " CLR_MAG(__TIME__) ")" << std::endl
	          << " Copyright 2014-2017, Institute for Automation of Complex Power Systems, EONERC" << std::endl
	          << " Steffen Vogel <StVogel@eonerc.rwth-aachen.de>" << std::endl;
}

void print_version()
{
	std::cout << PROJECT_BUILD_ID << std::endl;
}

ssize_t read_random(char *buf, size_t len)
{
	int fd;
	ssize_t bytes;

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

	info("Initialize signals");

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

} // namespace utils
} // namespace villas
