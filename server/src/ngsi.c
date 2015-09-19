/** Node type: FIWARE NGSI 9/10
 *
 * This file implements the NGSI protocol as a node type.
 *
 * @see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 **********************************************************************************/

#include <curl/curl.h>
#include <jansson.h>

#include "ngsi.h"
#include "utils.h"

/* NGSI Entity, Attribute & Metadata structure */

void ngsi_entity_dtor(void *ptr)
{
	struct ngsi_entity *entity = ptr;

	list_destroy(&entity->attributes);
	list_destroy(&entity->metadata);

	free(entity->name);
	free(entity->type);
}

void ngsi_attribute_dtor(void *ptr)
{
	struct ngsi_attribute *entity = ptr;

	free(entity->name);
	free(entity->type);
}

struct ngsi_entity * ngsi_entity()
{
	struct ngsi_entity *entity = alloc(sizeof(struct ngsi_entity));
	
	list_init(&entity->attributes, ngsi_attribute_dtor);
	list_init(&entity->metadata, ngsi_attribute_dtor);

	return entity;
}

/* Node type */

int ngsi_init(int argc, char *argv[], struct settings *set)
{
	 curl_global_init(CURL_GLOBAL_ALL);
}

int ngsi_deinit()
{
	curl_global_cleanup();
}

int ngsi_parse(config_setting_t *cfg, struct node *n)
{
	struct ngsi *i = n->ngsi;

	config_setting_t *mapping =  config_setting_get_member(cfg, "mapping");
	if (!mapping || !config_setting_is_array(mapping))
		cerror(cfg, "Missing entity.attribute mapping for node '%s", n->name);
	
	for (int i = 0; i < config_setting_length(mapping); i++) {
		const char *token = config_setting_get_string_elem(mapping, i);
		if (token) {
			if (sscanf(token, "%ms(%ms).%ms", &entity, &type, &attribue) != 2)
				cerror(mapping, "Invalid entity.attribute token: '%s'", token);
		
			
		}
		cerror(mapping, "Invalid entity.attribute token");
	}
}

int ngsi_print(struct node *n, char *buf, int len)
{
	struct ngsi *i = n->ngsi;
	
	return snprintf(buf, len, "endpoint=%s, token=%s, entities=%u",
		i->endpoint, i->token, i->entities);
}

int ngsi_open(struct node *n)
{
	char buf[128];
	struct ngsi *i = n->ngsi;

	i->curl = curl_easy_init();
	i->headers = NULL:
	
	if (i->token) {
		snprintf(buf, sizeof(buf), "Auth-Token: %s", i->token);
		i->headers = curl_slist_append(i->headers, buf);
	}
	
	i->headers = curl_slist_append(i->headers, "User-Agent: S2SS " VERSION);
	i->headers = curl_slist_append(i->headers, "Content-Type: application/json");
	
	snprintf(buf, sizeof(buf), "%s/v1/%s")

	curl_easy_setopt(i->curl, CURLOPT_URL, url);
	curl_easy_setopt(i->curl, CURLOPT_POSTFIELDSIZE, -1);
	curl_easy_setopt(i->curl, CURLOPT_HTTPHEADER, i->headers);
	
	/* Create entity */
	char *post = ngsi_update_context(n, "APPEND");
	
	return 0;
}

int ngsi_close(struct node *n)
{
	struct ngsi *i = n->ngsi;
	
	curl_easy_cleanup(i->curl);
	curl_slist_free_all(i->headers);
	
	return 0;
}

static int ngsi_query_context(struct node *n)
{
	struct ngsi *i = n->ngsi;
	
	return 0;
}

static char * ngsi_update_context(struct node *n, char *action)
{
	struct ngsi *i = n->ngsi;
	
	json_t *root = json_object();
	json_t *elements = json_array();
	
	for (int i = 0; i < i->entities; i++) {
		json_t *entity = json_object();
		json_t *attributes = json_object();

		json_object_set(entity, "type", json_string("Room"));
		json_object_set(entity, "isPattern", json_false());
		json_object_set(entity, "id", json_string("Room1"));
	}
	
	json_object_set(root, "contextElements", conextElements);
	json_object_set(root, "updateAction", json_string(action));
	
	return json_dumps(root, JSON_COMPACT);
}

int ngsi_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	
	return -1; /** @todo not yet implemented */
}

int ngsi_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct ngsi *i = n->ngsi;
	
	long result;
	char *post = ngsi_update_context(n, "UPDATE");
	
	curl_easy_setopt(i->curl, CURLOPT_COPYPOSTFIELDS, post);
	
	curl_easy_perform(i->curl);
	
	curl_easy_getinfo(i->curl, CURLINFO_RESPONSE_CODE, &result);
	
	return result == 200 ? 1 : 0
}

REGISTER_NODE_TYPE(NGSI, "ngsi", ngsi)