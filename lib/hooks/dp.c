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
	int index;
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
	double newest = *in;
	double oldest = window_update(&d->window, newest);

	for (int i = 0; i < d->fharmonics_len; i++) {
		double pi_fharm = 2.0 * M_PI * d->fharmonics[i];

		/* Recursive update */
		d->coeffs[i] = (d->coeffs[i] + (newest - oldest)) * cexp(pi_fharm);

		/* Correction for stationary phasor */
		double complex correction = cexp(pi_fharm * (d->t - (d->window.steps + 1)));
		double complex result = 2.0 / d->window.steps * d->coeffs[i] / correction;

		/* DC component */
		if (i == 0)
			result /= 2.0;

		out[i] = result;
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

	return 0;
}

static int dp_parse(struct hook *h, json_t *cfg)
{
	struct dp *d = (struct dp *) h->_vd;

	int ret;
	json_error_t err;
	json_t *json_harmonics, *json_harmonic;
	size_t i;

	double rate = -1, dt = -1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: F, s: F, s: o, s?: b }",
		"index", &d->index,
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

	d->fharmonics_len = json_array_size(json_harmonics);
	d->fharmonics = alloc(d->fharmonics_len * sizeof(double));
	d->coeffs = alloc(d->fharmonics_len * sizeof(double complex));
	if (!d->fharmonics || !d->coeffs)
		return -1;

	json_array_foreach(json_harmonics, i, json_harmonic) {
		if (!json_is_integer(json_harmonic))
			error("Setting 'harmonics' of hook '%s' must be a list of integers", plugin_name(h->_vt));

		d->fharmonics[i] = d->f0 * json_integer_value(json_harmonic);
	}

	return 0;
}

static int dp_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct dp *d = (struct dp *) h->_vd;

	for (unsigned j = 0; j < *cnt; j++) {
		struct sample *smp = smps[j];

		if (d->index > smp->length)
			continue;

		struct signal *s = (struct signal *) vlist_at(smp->signals, d->index);

		if (d->inverse) {
			if (s->type != SIGNAL_TYPE_FLOAT)
				return -1;

			dp_istep(d, &smp->data[d->index].z, &smp->data[d->index].f);
		}
		else {
			if (s->type != SIGNAL_TYPE_COMPLEX)
				return -1;

			dp_step(d, &smp->data[d->index].f, &smp->data[d->index].z);
		}
	}

	d->t += d->dt;

	return 0;
}

static struct plugin p = {
	.name		= "dp",
	.description	= "Transform to/from dynamic phasor domain",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH,
		.priority	= 99,
		.init		= dp_init,
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
