/** Dynamic Phasor Interface Algorithm hook.
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

#include <math.h>
#include <complex.h>
#include <string.h>

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/window.h>

#define J _Complex_I

struct dp {
	char *signal_name;
	int   signal_index;

	int offset;
	int inverse;

	double f0;
	double dt;
	double t;

	double complex *coeffs;
	int *fharmonics;
	int  fharmonics_len;

	struct window window;
};

static void dp_step(struct dp *d, double *in, float complex *out)
{
	int n = d->window.steps;
	double complex om, corr;
	double newest = *in;
	double oldest = window_update(&d->window, newest);

	for (int i = 0; i < d->fharmonics_len; i++) {
		om = 2.0 * M_PI * J * d->fharmonics[i] / n;

		/* Recursive update */
		d->coeffs[i] = cexp(om) * (d->coeffs[i] + (newest - oldest));

		/* Correction for stationary phasor */
		corr = cexp(-om * (d->t - (d->window.steps + 1)));

		out[i] = (2.0 / d->window.steps) * (d->coeffs[i] * corr);

		/* DC component */
		if (d->fharmonics[i] == 0)
			out[i] /= 2.0;
	}
}

static void dp_istep(struct dp *d, complex float *in, double *out)
{
	double complex value = 0;

	/* Reconstruct the original signal */
	for (int i = 0; i < d->fharmonics_len; i++) {
		double freq = d->fharmonics[i];
		double complex coeff = in[i];

		value += coeff * cexp(2.0 * M_PI * freq * d->t);
	}

	*out = creal(value);
}

static int dp_start(struct hook *h)
{
	int ret;
	struct dp *d = (struct dp *) h->_vd;

	d->t = 0;

	for (int i = 0; i < d->fharmonics_len; i++)
		d->coeffs[i] = 0;

	ret = window_init(&d->window, (1.0 / d->f0) / d->dt, 0.0);
	if (ret)
		return ret;

	return 0;
}

static int dp_stop(struct hook *h)
{
	int ret;
	struct dp *d = (struct dp *) h->_vd;

	ret = window_destroy(&d->window);
	if (ret)
		return ret;

	return 0;
}

static int dp_init(struct hook *h)
{
	struct dp *d = (struct dp *) h->_vd;

	/* Default values */
	d->inverse = 0;

	return 0;
}

static int dp_destroy(struct hook *h)
{
	struct dp *d = (struct dp *) h->_vd;

	/* Release memory */
	free(d->fharmonics);
	free(d->coeffs);

	if (d->signal_name)
		free(d->signal_name);

	return 0;
}

static int dp_parse(struct hook *h, json_t *cfg)
{
	struct dp *d = (struct dp *) h->_vd;

	int ret;
	json_error_t err;
	json_t *json_harmonics, *json_harmonic, *json_signal;
	size_t i;

	double rate = -1, dt = -1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s: F, s?: F, s?: F, s: o, s?: b }",
		"signal", &json_signal,
		"f0", &d->f0,
		"dt", &dt,
		"rate", &rate,
		"harmonics", &json_harmonics,
		"inverse", &d->inverse
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	if (rate > 0)
		d->dt = 1. / rate;
	else if (dt > 0)
		d->dt = dt;
	else
		error("Either on of the settings 'dt' or 'rate' must be gived for hook '%s'", plugin_name(h->_vt));

	if (!json_is_array(json_harmonics))
		error("Setting 'harmonics' of hook '%s' must be a list of integers", plugin_name(h->_vt));

	switch (json_typeof(json_signal)) {
		case JSON_STRING:
			d->signal_name = strdup(json_string_value(json_signal));
			break;

		case JSON_INTEGER:
			d->signal_name = NULL;
			d->signal_index = json_integer_value(json_signal);
			break;

		default:
			error("Invalid value for setting 'signal' in hook '%s'", hook_type_name(h->_vt));
	}

	d->fharmonics_len = json_array_size(json_harmonics);
	d->fharmonics = alloc(d->fharmonics_len * sizeof(double));
	d->coeffs = alloc(d->fharmonics_len * sizeof(double complex));
	if (!d->fharmonics || !d->coeffs)
		return -1;

	json_array_foreach(json_harmonics, i, json_harmonic) {
		if (!json_is_integer(json_harmonic))
			error("Setting 'harmonics' of hook '%s' must be a list of integers", plugin_name(h->_vt));

		d->fharmonics[i] = json_integer_value(json_harmonic);
	}

	return 0;
}

static int dp_prepare(struct hook *h)
{
	int ret;
	struct dp *d = (struct dp *) h->_vd;

	char *new_sig_name;
	struct signal *orig_sig, *new_sig;

	if (d->signal_name) {
		d->signal_index = vlist_lookup_index(&h->signals, d->signal_name);
		if (d->signal_index < 0)
			return -1;
	}

	if (d->inverse) {
		/* Remove complex-valued coefficient signals */
		for (int i = 0; i < d->fharmonics_len; i++) {
			orig_sig = vlist_at_safe(&h->signals, d->signal_index + i);
			if (!orig_sig)
				return -1;

			if (orig_sig->type != SIGNAL_TYPE_COMPLEX)
				return -1;

			ret = vlist_remove(&h->signals, d->signal_index + i);
			if (ret)
				return -1;

			signal_decref(orig_sig);
		}

		/* Add new real-valued reconstructed signals */
		new_sig = signal_create("dp", "idp", SIGNAL_TYPE_FLOAT);
		if (!new_sig)
			return -1;

		ret = vlist_insert(&h->signals, d->offset, new_sig);
		if (ret)
			return -1;
	}
	else {
		orig_sig = vlist_at_safe(&h->signals, d->signal_index);
		if (!orig_sig)
			return -1;

		if (orig_sig->type != SIGNAL_TYPE_FLOAT)
			return -1;

		ret = vlist_remove(&h->signals, d->signal_index);
		if (ret)
			return -1;

		for (int i = 0; i < d->fharmonics_len; i++) {
			new_sig_name = strf("%s_harm%d", orig_sig->name, i);

			new_sig = signal_create(new_sig_name, orig_sig->unit, SIGNAL_TYPE_COMPLEX);
			if (!new_sig)
				return -1;

			ret = vlist_insert(&h->signals, d->offset + i, new_sig);
			if (ret)
				return -1;
		}

		signal_decref(orig_sig);
	}

	return 0;
}

static int dp_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct dp *d = (struct dp *) h->_vd;

	for (unsigned j = 0; j < *cnt; j++) {
		struct sample *smp = smps[j];

		if (d->signal_index > smp->length)
			continue;

		if (d->inverse) {
			double signal;
			float complex *coeffs = &smp->data[d->signal_index].z;

			dp_istep(d, coeffs, &signal);

			sample_data_remove(smp, d->signal_index, d->fharmonics_len);
			sample_data_insert(smp, (union signal_data *) &signal, d->offset, 1);
		}
		else {
			double signal = smp->data[d->signal_index].f;
			float complex coeffs[d->fharmonics_len];

			dp_step(d, &signal, coeffs);

			sample_data_remove(smp, d->signal_index, 1);
			sample_data_insert(smp, (union signal_data *) coeffs, d->offset, d->fharmonics_len);
		}

		smp->signals = &h->signals;

		d->t += d->dt;
	}

	return 0;
}

static struct plugin p = {
	.name		= "dp",
	.description	= "Transform to/from dynamic phasor domain",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
		.priority	= 99,
		.init		= dp_init,
		.init_signals	= dp_prepare,
		.destroy	= dp_destroy,
		.start		= dp_start,
		.stop		= dp_stop,
		.parse		= dp_parse,
		.process	= dp_process,
		.size		= sizeof(struct dp)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
