/** Dynamic Phasor Interface Algorithm hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cmath>
#include <cstring>

#include <complex>

#include <villas/hook.hpp>
#include <villas/sample.h>
#include <villas/dsp/window.hpp>
#include <villas/utils.hpp>

using namespace std::complex_literals;
using namespace villas::utils;

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

	dsp::Window<double> window;

	void step(double *in, std::complex<float> *out)
	{
		int N = window.getLength();
		__attribute__((unused)) std::complex<double> om_k, corr;
		double newest = *in;
		__attribute__((unused)) double oldest = window.update(newest);

		for (int k = 0; k < fharmonics_len; k++) {
			om_k = 2.0i * M_PI * (double) fharmonics[k] / (double) N;

			/* Correction for stationary phasor */
			corr = std::exp(-om_k * (steps - (N + 1)));
			//corr = 1;

#if 0
			/* Recursive update */
			coeffs[k] = std::exp(om) * (coeffs[k] + (newest - oldest));

			out[k] = (2.0 / N) * (coeffs[i] * corr);

			/* DC component */
			if (fharmonics[k] == 0)
				out[k] /= 2.0;
#else
			/* Full DFT */
			std::complex<double> X_k = 0;

			for (int n = 0; n < N; n++) {
				double x_n = window[n];

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

	DPHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		signal_name(nullptr),
		signal_index(0),
		offset(0),
		inverse(0),
		f0(50.0),
		timestep(50e-6),
		time(),
		steps(0),
		coeffs(),
		fharmonics(),
		fharmonics_len(0)
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
		assert(state == State::PREPARED);

		time = 0;
		steps = 0;

		for (int i = 0; i < fharmonics_len; i++)
			coeffs[i] = 0;

		window = dsp::Window<double>((1.0 / f0) / timestep, 0.0);

		state = State::STARTED;
	}

	virtual void parse(json_t *json)
	{
		int ret;
		json_error_t err;
		json_t *json_harmonics, *json_harmonic, *json_signal;
		size_t i;

		Hook::parse(json);

		double rate = -1, dt = -1;

		ret = json_unpack_ex(json, &err, 0, "{ s: o, s: F, s?: F, s?: F, s: o, s?: b }",
			"signal", &json_signal,
			"f0", &f0,
			"dt", &dt,
			"rate", &rate,
			"harmonics", &json_harmonics,
			"inverse", &inverse
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-dp");

		if (rate > 0)
			timestep = 1. / rate;
		else if (dt > 0)
			timestep = dt;
		else
			throw ConfigError(json, "node-config-hook-dp", "Either on of the settings 'dt' or 'rate' must be given");

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
			throw MemoryAllocationError();

		json_array_foreach(json_harmonics, i, json_harmonic) {
			if (!json_is_integer(json_harmonic))
				throw ConfigError(json_harmonic, "node-config-hook-dp-harmonics", "Setting 'harmonics' must be a list of integers");

			fharmonics[i] = json_integer_value(json_harmonic);
		}

		state = State::PARSED;
	}

	virtual void prepare()
	{
		int ret;

		assert(state == State::CHECKED);

		char *new_sig_name;
		struct signal *orig_sig, *new_sig;

		assert(state != State::STARTED);

		if (signal_name) {
			signal_index = vlist_lookup_index<struct signal>(&signals, signal_name);
			if (signal_index < 0)
				throw RuntimeError("Failed to find signal: {}", signal_name);
		}

		if (inverse) {
			/* Remove complex-valued coefficient signals */
			for (int i = 0; i < fharmonics_len; i++) {
				orig_sig = (struct signal *) vlist_at_safe(&signals, signal_index + i);
				if (!orig_sig)
					throw RuntimeError("Failed to find signal");;

				if (orig_sig->type != SignalType::COMPLEX)
					throw RuntimeError("Signal is not complex");;

				ret = vlist_remove(&signals, signal_index + i);
				if (ret)
					throw RuntimeError("Failed to remove signal from list");;

				signal_decref(orig_sig);
			}

			/* Add new real-valued reconstructed signals */
			new_sig = signal_create("dp", "idp", SignalType::FLOAT);
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

			if (orig_sig->type != SignalType::FLOAT)
				throw RuntimeError("Signal is not float");;

			ret = vlist_remove(&signals, signal_index);
			if (ret)
				throw RuntimeError("Failed to remove signal from list");;

			for (int i = 0; i < fharmonics_len; i++) {
				new_sig_name = strf("%s_harm%d", orig_sig->name, i);

				new_sig = signal_create(new_sig_name, orig_sig->unit, SignalType::COMPLEX);
				if (!new_sig)
					throw RuntimeError("Failed to create new signal");;

				ret = vlist_insert(&signals, offset + i, new_sig);
				if (ret)
					throw RuntimeError("Failed to insert signal into list");;
			}

			signal_decref(orig_sig);
		}

		state = State::PREPARED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		if (signal_index > smp->length)
			return Hook::Reason::ERROR;

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

		time += timestep;
		steps++;

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "dp";
static char d[] = "Transform to/from dynamic phasor domain";
static HookPlugin<DPHook, n, d, (int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE> p;

} /* namespace node */
} /* namespace villas */

/** @} */
