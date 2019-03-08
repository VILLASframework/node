/** Average hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/bitset.h>

struct average {
	uint64_t mask;
	int offset;

	struct bitset mask;
	struct vlist signal_names;
};

static int average_init(struct hook *h)
{
	int ret;
	struct average *a = (struct average *) h->_vd;

	ret = vlist_init(&a->signal_names);
	if (ret)
		return ret;

	ret = bitset_init(&a->mask, 128);
	if (ret)
		return ret;

	bitset_clear_all(&a->mask);

	return 0;
}

static int average_destroy(struct hook *h)
{
	int ret;
	struct average *a = (struct average *) h->_vd;

	ret = vlist_destroy(&a->signal_names, NULL, true);
	if (ret)
		return ret;

	ret = bitset_destroy(&a->mask);
	if (ret)
		return ret;

	return 0;
}

static int average_prepare(struct hook *h)
{
	int ret;
	struct average *a = (struct average *) h->_vd;
	struct signal *avg_sig;

	/* Setup mask */
	for (size_t i = 0; i < vlist_length(&a->signal_names); i++) {
		char *signal_name = (char *) vlist_at_safe(&a->signal_names, i);

		int index = vlist_lookup_index(&a->signal_names, signal_name);
		if (index < 0)
			return -1;

		bitset_set(&a->mask, index);
	}

	/* Add averaged signal */
	avg_sig = signal_create("average", NULL, SIGNAL_TYPE_FLOAT);
	if (!avg_sig)
		return -1;

	ret = vlist_insert(&h->signals, a->offset, avg_sig);
	if (ret)
		return ret;

	return 0;
}

static int average_parse(struct hook *h, json_t *cfg)
{
	struct average *a = (struct average *) h->_vd;

	int ret;
	size_t i;
	json_error_t err;
	json_t *json_signals, *json_signal;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: o }",
		"offset", &a->offset,
		"signals", &json_signals
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	if (!json_is_array(json_signals))
		error("Setting 'signals' of hook '%s' must be a list of signal names", hook_type_name(h->_vt));

	json_array_foreach(json_signals, i, json_signal) {
		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				vlist_push(&a->signal_names, strdup(json_string_value(json_signal)));
				break;

			case JSON_INTEGER:
				bitset_set(&a->mask, json_integer_value(json_signal));
				break;

			default:
				error("Invalid value for setting 'signals' in hook '%s'", hook_type_name(h->_vt));
		}
	}

	return 0;
}

static int average_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct average *a = (struct average *) h->_vd;

	for (int i = 0; i < *cnt; i++) {
		struct sample *smp = smps[i];
		double avg, sum = 0;
		int n = 0;

		for (int k = 0; k < smp->length; k++) {
			if (!bitset_test(&a->mask, k))
				continue;

			switch (sample_format(smp, k)) {
				case SIGNAL_TYPE_INTEGER:
					sum += smp->data[k].i;
					break;

				case SIGNAL_TYPE_FLOAT:
					sum += smp->data[k].f;
					break;

				case SIGNAL_TYPE_AUTO:
				case SIGNAL_TYPE_INVALID:
				case SIGNAL_TYPE_COMPLEX:
				case SIGNAL_TYPE_BOOLEAN:
					return -1; /* not supported */
			}

			n++;
		}

		avg = n == 0 ? 0 : sum / n;
		sample_data_insert(smp, (union signal_data *) &avg, a->offset, 1);
		smp->signals = &h->signals;
	}

	return 0;
}

static struct plugin p = {
	.name		= "average",
	.description	= "Calculate average over some signals",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
		.priority	= 99,
		.parse		= average_parse,
		.process	= average_process,
		.init		= average_init,
		.init_signals	= average_prepare,
		.destroy	= average_destroy,
		.size		= sizeof(struct average)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
