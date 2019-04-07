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
#include <string.h>

#include <complex>

#include <villas/hook.hpp>
#include <villas/sample.h>
#include <villas/window.h>
#include <villas/utils.h>

using namespace std::complex_literals;

namespace villas {
namespace node {

class DPHook : public Hook {

protected:
	char *signal_name;
	unsigned signal_index;

	int offset;
	int inverse;

	double f0;
	double timestep;
	double time;
	double steps;

	std::complex<double> *coeffs;
	int *fharmonics;
	int  fharmonics_len;

	struct window window;

	void step(double *in, std::complex<float> *out)
	{
		int N = window.steps;
		__attribute__((unused)) std::complex<double> om_k, corr;
		double newest = *in;
		__attribute__((unused)) double oldest = window_update(&window, newest);

		for (int k = 0; k < fharmonics_len; k++) {
			om_k = 2.0i * M_PI * (double) fharmonics[k] / (double) N;

			/* Correction for stationary phasor */
			corr = std::exp(-om_k * (steps - (window.steps + 1)));
			//corr = 1;

#if 0
			/* Recursive update */
			coeffs[k] = std::exp(om) * (coeffs[k] + (newest - oldest));

			out[k] = (2.0 / window.steps) * (coeffs[i] * corr);

			/* DC component */
			if (fharmonics[k] == 0)
				out[k] /= 2.0;
#else
			/* Full DFT */
			std::complex<double> X_k = 0;

			for (int n = 0; n < N; n++) {
				double x_n = window.data[(window.pos - window.steps + n) & window.mask];

				X_k += x_n * std::exp(om_k * (double) n);
			}

			out[k] = X_k / (corr * (double) N);
#endif
		}
	}

	void istep(std::complex<float> *in, double *out)
	{
		std::complex<double> value = 0;

		/* Reconstruct the original signal */
		for (int k = 0; k < fharmonics_len; k++) {
			double freq = fharmonics[k];
			std::complex<double> coeff = in[k];
			std::complex<double> om = 2.0i * M_PI * freq * time;

			value += coeff * std::exp(om);
		}

		*out = std::real(value);
	}

public:

	DPHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		inverse(0)
	{ }

	virtual ~DPHook()
	{
		/* Release memory */
		if (fharmonics)
			delete fharmonics;

		if (coeffs)
			delete coeffs;

		if (signal_name)
			free(signal_name);
	}

	virtual void start()
	{
		int ret;

		assert(state == STATE_PREPARED);

		time = 0;
		steps = 0;

		for (int i = 0; i < fharmonics_len; i++)
			coeffs[i] = 0;

		ret = window_init(&window, (1.0 / f0) / timestep, 0.0);
		if (ret)
			throw RuntimeError("Failed to initialize window");

		state = STATE_STARTED;
	}

	virtual void stop()
	{
		int ret;

		assert(state == STATE_STARTED);

		ret = window_destroy(&window);
		if (ret)
			throw RuntimeError("Failed to destroy window");

		state = STATE_STOPPED;
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		json_error_t err;
		json_t *json_harmonics, *json_harmonic, *json_signal;
		size_t i;

		double rate = -1, dt = -1;

		ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s: F, s?: F, s?: F, s: o, s?: b }",
			"signal", &json_signal,
			"f0", &f0,
			"dt", &dt,
			"rate", &rate,
			"harmonics", &json_harmonics,
			"inverse", &inverse
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-dp");

		if (rate > 0)
			timestep = 1. / rate;
		else if (dt > 0)
			timestep = dt;
		else
			throw ConfigError(cfg, "node-config-hook-dp", "Either on of the settings 'dt' or 'rate' must be given");

		if (!json_is_array(json_harmonics))
			throw ConfigError(json_harmonics, "node-config-hook-dp-harmonics", "Setting 'harmonics' must be a list of integers");

		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				signal_name = strdup(json_string_value(json_signal));
				break;

			case JSON_INTEGER:
				signal_name = nullptr;
				signal_index = json_integer_value(json_signal);
				break;

			default:
				throw ConfigError(json_signal, "node-config-hook-dp-signal", "Invalid value for setting 'signal'");
		}

		fharmonics_len = json_array_size(json_harmonics);
		fharmonics = new int[fharmonics_len];
		coeffs = new std::complex<double>[fharmonics_len];
		if (!fharmonics || !coeffs)
			throw RuntimeError("Failed to allocate memory");

		json_array_foreach(json_harmonics, i, json_harmonic) {
			if (!json_is_integer(json_harmonic))
				throw ConfigError(json_harmonic, "node-config-hook-dp-harmonics", "Setting 'harmonics' must be a list of integers");

			fharmonics[i] = json_integer_value(json_harmonic);
		}

		state = STATE_PARSED;
	}

	virtual void prepare()
	{
		int ret;

		assert(state == STATE_CHECKED);

		char *new_sig_name;
		struct signal *orig_sig, *new_sig;

		assert(state != STATE_STARTED);

		if (signal_name) {
			signal_index = vlist_lookup_index(&signals, signal_name);
			if (signal_index < 0)
				throw RuntimeError("Failed to find signal: {}", signal_name);
		}

		if (inverse) {
			/* Remove complex-valued coefficient signals */
			for (int i = 0; i < fharmonics_len; i++) {
				orig_sig = (struct signal *) vlist_at_safe(&signals, signal_index + i);
				if (!orig_sig)
					throw RuntimeError("Failed to find signal");;

				if (orig_sig->type != SIGNAL_TYPE_COMPLEX)
					throw RuntimeError("Signal is not complex");;

				ret = vlist_remove(&signals, signal_index + i);
				if (ret)
					throw RuntimeError("Failed to remove signal from list");;

				signal_decref(orig_sig);
			}

			/* Add new real-valued reconstructed signals */
			new_sig = signal_create("dp", "idp", SIGNAL_TYPE_FLOAT);
			if (!new_sig)
				throw RuntimeError("Failed to create signal");;

			ret = vlist_insert(&signals, offset, new_sig);
			if (ret)
				throw RuntimeError("Failed to insert signal into list");;
		}
		else {
			orig_sig = (struct signal *) vlist_at_safe(&signals, signal_index);
			if (!orig_sig)
				throw RuntimeError("Failed to find signal");;

			if (orig_sig->type != SIGNAL_TYPE_FLOAT)
				throw RuntimeError("Signal is not float");;

			ret = vlist_remove(&signals, signal_index);
			if (ret)
				throw RuntimeError("Failed to remove signal from list");;

			for (int i = 0; i < fharmonics_len; i++) {
				new_sig_name = strf("%s_harm%d", orig_sig->name, i);

				new_sig = signal_create(new_sig_name, orig_sig->unit, SIGNAL_TYPE_COMPLEX);
				if (!new_sig)
					throw RuntimeError("Failed to create new signal");;

				ret = vlist_insert(&signals, offset + i, new_sig);
				if (ret)
					throw RuntimeError("Failed to insert signal into list");;
			}

			signal_decref(orig_sig);
		}

		state = STATE_PREPARED;
	}

	virtual int process(sample *smp)
	{
		if (signal_index > smp->length)
			return HOOK_ERROR;

		if (inverse) {
			double signal;
			std::complex<float> *coeffs = reinterpret_cast<std::complex<float> *>(&smp->data[signal_index].z);

			istep(coeffs, &signal);

			sample_data_remove(smp, signal_index, fharmonics_len);
			sample_data_insert(smp, (union signal_data *) &signal, offset, 1);
		}
		else {
			double signal = smp->data[signal_index].f;
			std::complex<float> coeffs[fharmonics_len];

			step(&signal, coeffs);

			sample_data_remove(smp, signal_index, 1);
			sample_data_insert(smp, (union signal_data *) coeffs, offset, fharmonics_len);
		}

		smp->signals = &signals;

		time += timestep;
		steps++;

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<DPHook> p(
	"dp",
	"Transform to/from dynamic phasor domain",
	HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
