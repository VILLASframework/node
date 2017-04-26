/** Logging routines that depend on libconfig.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/
 
#pragma once

struct log;

#include <libconfig.h>

#include "log.h"

/** Parse logging configuration. */
int log_parse(struct log *l, config_setting_t *cfg);

/** Print configuration error and exit. */
void cerror(config_setting_t *cfg, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

