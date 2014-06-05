/**
 * Configuration parser
 *
 * @author Steffen Vogel <steffen.vogel@rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <libconfig.h>

#include "path.h"
#include "node.h"

int config_parse(config_t *c, const char *filename, struct path *paths, struct node *nodes);

int config_parse_path(config_setting_t *c, struct path *p);
int config_parse_node(config_setting_t *c, struct node *n);

#endif /* _CONFIG_H_ */
