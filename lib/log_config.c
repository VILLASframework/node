/** Logging routines that depend on libconfig.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "log_config.h"
#include "utils.h"

int log_parse(struct log *l, config_setting_t *cfg)
{
	const char *fac, *pth;
	int lvl;

	if (!config_setting_is_group(cfg))
		cerror(cfg, "Setting 'log' must be a group.");

	if (config_setting_lookup_int(cfg, "level", &lvl))
		l->level = lvl;

	if (config_setting_lookup_string(cfg, "file", &pth))
		l->path = pth;

	if (config_setting_lookup_string(cfg, "facilities", &fac))
		log_set_facility_expression(l, fac);

	l->state = STATE_PARSED;

	return 0;
}

void cerror(config_setting_t *cfg, const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;
	const char *file;
	int line;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	line = config_setting_source_line(cfg);
	file = config_setting_source_file(cfg);
	if (!file)
		file = config_setting_get_hook(config_root_setting(cfg->config));

	log_print(l, LOG_LVL_ERROR, "%s in %s:%u", buf, file, line);

	free(buf);

	killme(SIGABRT);
	pause();
}
