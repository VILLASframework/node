/** Sample value remapping for mux.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>

#include "mapping.h"
#include "stats.h"
#include "sample.h"
#include "list.h"
#include "utils.h"

int mapping_entry_parse_str(struct mapping_entry *e, const char *str)
{
	char *cpy, *type, *field, *subfield, *end;
	
	cpy = strdup(str);
	if (!cpy)
		return -1;

	type = strtok(cpy, ".[");
	if (!type)
		goto invalid_format;

	if      (!strcmp(type, "stats")) {
		e->type = MAPPING_TYPE_STATS;
		e->length = 1;
		
		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;
		
		subfield = strtok(NULL, ".");
		if (!subfield)
			goto invalid_format;
		
		end = strtok(NULL, ".");
		if (end)
			goto invalid_format;

		e->stats.id = stats_lookup_id(field);
		if (e->stats.id < 0)
			goto invalid_format;
		
		if      (!strcmp(subfield, "total"))
			e->stats.type = MAPPING_STATS_TYPE_TOTAL;
		else if (!strcmp(subfield, "last"))
			e->stats.type = MAPPING_STATS_TYPE_LAST;
		else if (!strcmp(subfield, "lowest"))
			e->stats.type = MAPPING_STATS_TYPE_LOWEST;
		else if (!strcmp(subfield, "highest"))
			e->stats.type = MAPPING_STATS_TYPE_HIGHEST;
		else if (!strcmp(subfield, "mean"))
			e->stats.type = MAPPING_STATS_TYPE_MEAN;
		else if (!strcmp(subfield, "var"))
			e->stats.type = MAPPING_STATS_TYPE_VAR;
		else if (!strcmp(subfield, "stddev"))
			e->stats.type = MAPPING_STATS_TYPE_STDDEV;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "hdr")) {
		e->type = MAPPING_TYPE_HEADER;
		e->length = 1;
		
		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;
		
		end = strtok(NULL, ".");
		if (end)
			goto invalid_format;
		
		if      (!strcmp(field, "sequence"))
			e->header.id = MAPPING_HEADER_SEQUENCE;
		else if (!strcmp(field, "length"))
			e->header.id = MAPPING_HEADER_LENGTH;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "ts")) {
		e->type = MAPPING_TYPE_TIMESTAMP;
		e->length = 2;

		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;

		end = strtok(NULL, ".");
		if (end)
			goto invalid_format;
		
		if      (!strcmp(field, "origin"))
			e->timestamp.id = MAPPING_TIMESTAMP_ORIGIN;
		else if (!strcmp(field, "received"))
			e->timestamp.id = MAPPING_TIMESTAMP_RECEIVED;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "data")) {
		char *first_str, *last_str, *endptr;
		int first, last;
		
		e->type = MAPPING_TYPE_DATA;
		
		first_str = strtok(NULL, "-]");
		if (!first_str)
			goto invalid_format;
		
		last_str = strtok(NULL, "]");
		if (!last_str)
			last_str = first_str; /* single element: data[5] => data[5-5] */
		
		end = strtok(NULL, ".");
		if (end)
			goto invalid_format;
		
		first = strtoul(first_str, &endptr, 10);
		if (endptr != first_str + strlen(first_str))
			goto invalid_format;
		
		last = strtoul(last_str, &endptr, 10);
		if (endptr != last_str + strlen(last_str))
			goto invalid_format;
		
		if (last < first)
			goto invalid_format;
		
		e->data.offset = first;
		e->length = last - first + 1;
	}
	else
		goto invalid_format;

	free(cpy);
	return 0;

invalid_format:
	
	free(cpy);
	return -1;
}

int mapping_entry_parse(struct mapping_entry *e, config_setting_t *cfg)
{
	const char *str;

	str = config_setting_get_string(cfg);
	if (!str)
		return -1;
	
	return mapping_entry_parse_str(e, str);
}

int mapping_init(struct mapping *m)
{
	assert(m->state == STATE_DESTROYED);
	
	list_init(&m->entries);
	
	m->state = STATE_INITIALIZED;
	
	return 0;
}

int mapping_destroy(struct mapping *m)
{
	assert(m->state != STATE_DESTROYED);
	
	list_destroy(&m->entries, NULL, true);
	
	m->state = STATE_DESTROYED;
	
	return 0;
}

int mapping_parse(struct mapping *m, config_setting_t *cfg)
{
	int ret;
	
	assert(m->state == STATE_INITIALIZED);
	
	if (!config_setting_is_array(cfg))
		return -1;

	m->real_length = 0;

	for (int i = 0; i < config_setting_length(cfg); i++) {
		struct mapping_entry e;
		config_setting_t *cfg_mapping;
		
		cfg_mapping = config_setting_get_elem(cfg, i);
		if (!cfg_mapping)
			return -1;
		
		ret = mapping_entry_parse(&e, cfg_mapping);
		if (ret)
			return ret;	
		
		list_push(&m->entries, memdup(&e, sizeof(e)));
		
		m->real_length += e.length;
	}
	
	m->state = STATE_PARSED;
	
	return 0;
}

int mapping_remap(struct mapping *m, struct sample *original, struct sample *remapped, struct stats *s)
{
	int k = 0;
	
	if (remapped->capacity < m->real_length)
		return -1;

	/* We copy all the header fields */
	remapped->sequence = original->sequence;
	remapped->pool     = original->pool;
	remapped->source   = original->source;
	remapped->ts       = original->ts;
	remapped->format   = 0;
	remapped->length   = 0;

	for (size_t i = 0; i < list_length(&m->entries); i++) {
		struct mapping_entry *e = list_at(&m->entries, i);

		switch (e->type) {
			case MAPPING_TYPE_STATS: {
				struct hist *h = &s->histograms[e->stats.id];
				
				switch (e->stats.type) {
					case MAPPING_STATS_TYPE_TOTAL:
						sample_set_data_format(remapped, k, SAMPLE_DATA_FORMAT_INT);
						remapped->data[k++].f = h->total;
						break;
					case MAPPING_STATS_TYPE_LAST:
						remapped->data[k++].f = h->last;
						break;
					case MAPPING_STATS_TYPE_HIGHEST:
						remapped->data[k++].f = h->highest;
						break;
					case MAPPING_STATS_TYPE_LOWEST:
						remapped->data[k++].f = h->lowest;
						break;
					case MAPPING_STATS_TYPE_MEAN:
						remapped->data[k++].f = hist_mean(h);
						break;
					case MAPPING_STATS_TYPE_STDDEV:
						remapped->data[k++].f = hist_stddev(h);
						break;
					case MAPPING_STATS_TYPE_VAR:
						remapped->data[k++].f = hist_var(h);
						break;
					default:
						return -1;
				}
			}

			case MAPPING_TYPE_TIMESTAMP: {
				struct timespec *ts;
				
				switch (e->timestamp.id) {
					case MAPPING_TIMESTAMP_RECEIVED:
						ts = &original->ts.received;
						break;
					case MAPPING_TIMESTAMP_ORIGIN:
						ts = &original->ts.origin;
						break;
					default:
						return -1;
				}
				
				sample_set_data_format(remapped, k,   SAMPLE_DATA_FORMAT_INT);
				sample_set_data_format(remapped, k+1, SAMPLE_DATA_FORMAT_INT);

				remapped->data[k++].i = ts->tv_sec;
				remapped->data[k++].i = ts->tv_nsec;
				
				break;
			}

			case MAPPING_TYPE_HEADER:
				sample_set_data_format(remapped, k, SAMPLE_DATA_FORMAT_INT);
			
				switch (e->header.id) {
					case MAPPING_HEADER_LENGTH:
						remapped->data[k++].i = original->length;
						break;
					case MAPPING_HEADER_SEQUENCE:
						remapped->data[k++].i = original->sequence;
						break;
					default:
						return -1;
				}
				break;
				
			case MAPPING_TYPE_DATA:
				for (int j = e->data.offset; j < e->length + e->data.offset; j++) {
					int f = sample_get_data_format(original, j);
					
					if (j >= original->length) {
						sample_set_data_format(remapped, k, SAMPLE_DATA_FORMAT_FLOAT);
						remapped->data[k++].f = 0;
					}
					else {
						sample_set_data_format(remapped, k, f);
						remapped->data[k++] = original->data[j];
					}
				}
				break;
		}
		
		remapped->length += e->length;
	}

	return 0;
}