#pragma once
/** JSON serializtion of various objects.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifdef WITH_JANSSON

#include <jansson.h>
#include <libconfig.h>

#include "sample.h"

/* Convert a libconfig object to a libjansson object */
json_t * config_to_json(config_setting_t *cfg);

int json_to_config(json_t *json, config_setting_t *parent);

int sample_io_json_pack(json_t **j, struct sample *s, int flags);

int sample_io_json_unpack(json_t *j, struct sample *s, int *flags);

int sample_io_json_fprint(FILE *f, struct sample *s, int flags);

int sample_io_json_fscan(FILE *f, struct sample *s, int *flags);
#endif
