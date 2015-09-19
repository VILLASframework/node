/** Node type: NGSI 9/10 (FIWARE context broker)
 *
 * This file implements the NGSI protocol as a node type.
 *
 * @see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup ngsi FIRWARE NGSI 9/10 RESTful HTTP API
 * @ingroup node
 * @{
 **********************************************************************************/

#ifndef _NGSI_H_
#define _NGSI_H_

#include <curl/curl.h>

#include "list.h"
#include "config.h"
#include "msg.h"
#include "cfg.h"
#include "node.h"

struct node;

struct ngsi_attribute {
	char *name;
	char *type;
	
	int index;
};

struct ngsi_entity {
	char *name;
	char *type;

	struct list attributes;
	struct list metadata;
};

struct ngsi {
	char *endpoint;
	char *token;
	
	struct list entities;
	
	struct curl_slist *headers;	
	CURL *curl;
};

/** Initialize global NGSI settings and maps shared memory regions.
 *
 * @see node_vtable::init
 */
int ngsi_init(int argc, char *argv[], struct settings *set);

/** Free global NGSI settings and unmaps shared memory regions.
 *
 * @see node_vtable::deinit
 */
int ngsi_deinit();

/** @see node_vtable::parse */
int ngsi_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::print */
int ngsi_print(struct node *n, char *buf, int len);

/** @see node_vtable::open */
int ngsi_open(struct node *n);

/** @see node_vtable::close */
int ngsi_close(struct node *n);

/** @see node_vtable::read */
int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::write */
int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

#endif /** _NGSI_H_ @} */