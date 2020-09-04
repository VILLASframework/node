/** DFT hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
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

#include <cstring>

#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/plugin.h>
#include <complex>
#include <iostream>
#include <fstream>

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	enum paddingType {
		ZERO,
		SIG_REPEAT
	};

	enum windowType {
		NONE,
		FLATTOP,
		HANN,
		HAMMING
	};

	windowType window_type;
	paddingType padding_type;

	struct format_type *format;

	double* smp_memory;
	std::complex<double>** dftMatrix;
	std::complex<double>* dftResults;
	double* filterWindowCoefficents;
	double* absDftResults;
	double* absDftFreqs;

	uint sample_rate;
	double start_freqency;
	double end_freqency;
	double frequency_resolution;
	uint dft_rate;
	uint window_size;
	uint window_multiplier;//multiplyer for the window to achieve frequency resolution
	uint freq_count;//number of requency bins that are calculated
	bool sync_dft;

	uint smp_mem_pos;
	uint64_t last_sequence;
	
	
	std::complex<double> omega;
	std::complex<double> M_I;


	double window_corretion_factor;
	timespec last_dft_cal;
	


public:
	DftHook(struct vpath *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		window_type(windowType::NONE),
		padding_type(paddingType::ZERO),
		sample_rate(0),
		start_freqency(0),
		end_freqency(0),
		frequency_resolution(0),
		dft_rate(0),
		window_size(0),
		window_multiplier(0),
		freq_count(0),
		sync_dft(0),
		smp_mem_pos(0),
		last_sequence(0),
		M_I(0.0,1.0),
		window_corretion_factor(0),
		last_dft_cal({0,0})
	{
		format = format_type_lookup("villas.human");
	}

	virtual void prepare(){

		struct signal *freq_sig;
		struct signal *ampl_sig;
		struct signal *phase_sig;

		/* Add signals */
		freq_sig = signal_create("amplitude", nullptr, SignalType::FLOAT);
		ampl_sig = signal_create("phase", nullptr, SignalType::FLOAT);
		phase_sig = signal_create("frequency", nullptr, SignalType::FLOAT);


		if (!freq_sig || !ampl_sig || !phase_sig)
			throw RuntimeError("Failed to create new signals");


		vlist_push(&signals, freq_sig);
		vlist_push(&signals, ampl_sig);
		vlist_push(&signals, phase_sig);

		
		//offset = vlist_length(&signals) - 1;//needs to be cleaned up


		window_multiplier = ceil( ( (double)sample_rate / window_size ) / frequency_resolution);//calculate how much zero padding ist needed for a needed resolution


		freq_count = ceil( ( end_freqency - start_freqency ) / frequency_resolution) + 1;

		//init sample memory
		smp_memory = new double[window_size];
		if (!smp_memory)
			throw MemoryAllocationError();

		for(uint i = 0; i < window_size; i++)
			smp_memory[i] = 0;



		//init matrix of dft coeffients
		dftMatrix = new std::complex<double>*[freq_count];
		if (!dftMatrix)
			throw MemoryAllocationError();

		for(uint i = 0; i < freq_count; i++) {
			dftMatrix[i] = new std::complex<double>[window_size * window_multiplier]();
			if (!dftMatrix[i])
				throw MemoryAllocationError();
		}

		dftResults = new std::complex<double>[freq_count]();

		filterWindowCoefficents = new double[window_size];

		absDftResults = new double[freq_count];
		
		absDftFreqs = new double[freq_count];
		for(uint i=0; i < freq_count; i++)
			absDftFreqs[i] = start_freqency + i * frequency_resolution;

		genDftMatrix();
		calcWindow(window_type);


		state = State::PREPARED;

	}


	virtual void start()
	{

		assert(state == State::PREPARED || state == State::STOPPED);


		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);


		state = State::STOPPED;
	}

	virtual void parse(json_t *cfg)
	{
		const char *padding_type_c = nullptr, *window_type_c = nullptr;
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		state = State::PARSED;

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: i , s?: i, s?: s, s?: s, s?: b}",
			"sample_rate", &sample_rate,
			"start_freqency", &start_freqency,
			"end_freqency", &end_freqency,
			"frequency_resolution", &frequency_resolution,
			"dft_rate", &dft_rate,
			"window_size", &window_size,
			"window_type", &window_type_c,
			"padding_type", &padding_type_c,
			"sync", &sync_dft

		);

		if(!window_type_c) {
			info("No Window type given, assume no windowing");
			window_type = windowType::NONE;
		} else if(strcmp(window_type_c, "flattop") == 0)
			window_type = windowType::FLATTOP;
		else if(strcmp(window_type_c, "hamming") == 0)
			window_type = windowType::HAMMING;
		else if(strcmp(window_type_c, "hann") == 0)
			window_type = windowType::HANN;
		else {
			info("Window type %s not recognized, assume no windowing",window_type_c);
			window_type = windowType::NONE;
		}

		if(!padding_type_c) {
			info("No Padding type given, assume no zeropadding");
			padding_type = paddingType::ZERO;
		} else if(strcmp(padding_type_c, "signal_repeat") == 0) {
			padding_type = paddingType::SIG_REPEAT;
		} else {
			info("Padding type %s not recognized, assume zero padding",padding_type_c);
			padding_type = paddingType::ZERO;
		}	


		if(end_freqency < 0 || end_freqency > sample_rate){
			error("End frequency must be smaller than sample_rate (%i)",sample_rate);
			ret = 1;
		}

		if(frequency_resolution > ((double)sample_rate/window_size)){
			error("The maximum frequency resolution with smaple_rate:%i and window_site:%i is %f",sample_rate, window_size, ((double)sample_rate/window_size) );
			ret = 1;
		}



		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-dft");
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		smp_memory[smp_mem_pos % window_size] = smp->data[0].f;
		smp_mem_pos ++ ;


		double timediff = 0;
		if( sync_dft ){
			timediff = ( smp->ts.origin.tv_sec - last_dft_cal.tv_sec ) + ( smp->ts.origin.tv_nsec - last_dft_cal.tv_nsec ) * 1e-9;
		}

		if(	(( !sync_dft && ( smp_mem_pos % ( sample_rate / dft_rate )) == 0 ) ) || (sync_dft && timediff >= ( 1 / dft_rate ) ) ) {
			calcDft(paddingType::ZERO);
			double maxF = 0;
			double maxA = 0;
			int maxPos = 0;

			for(uint i=0; i<freq_count; i++){
				absDftResults[i] = abs(dftResults[i]) * 2 / (window_size * window_corretion_factor * ((padding_type == paddingType::ZERO)?1:window_multiplier) );
				if(maxA < absDftResults[i]){
					maxF = absDftFreqs[i];
					maxA = absDftResults[i];
					maxPos = i;
				}
			}
			info("sec=%ld, nsec=%ld freq: %f \t phase: %f \t amplitude: %f",last_dft_cal.tv_sec, smp->ts.origin.tv_nsec, maxF, atan2(dftResults[maxPos].imag(), dftResults[maxPos].real()), (maxA / pow(2,1./2)) );
			

			dumpData("/tmp/absDftResults", absDftResults, freq_count, absDftFreqs);

			last_dft_cal = smp->ts.origin;
		}

		
		if((smp->sequence - last_sequence) > 1 )
			warning("Calculation is not Realtime. %li sampled missed",smp->sequence - last_sequence);

		last_sequence = smp->sequence;

		return Reason::OK;
	}

	virtual ~DftHook()
	{
		//delete smp_memory;
	}

	void dumpData(const char *path, double *ydata, uint size, double *xdata=nullptr){
		
		std::ofstream fh;
		fh.open(path);
		for(uint i = 0 ; i < size ; i++){
			if(i>0)fh << ";";
			fh << ydata[i];
		}
		if(xdata){
			fh << "\n";
			for(uint i = 0 ; i < size ; i++){
				if(i>0)fh << ";";
				fh << xdata[i];
			}
		}
		fh.close();
	}


	void genDftMatrix(){
		using namespace std::complex_literals;

		omega = exp((-2 * M_PI * M_I) / (double)(window_size * window_multiplier));
		uint startBin = floor( start_freqency / frequency_resolution );
	

		for( uint i = 0; i <  freq_count ; i++){
			for( uint j=0 ; j < window_size * window_multiplier ; j++){
				dftMatrix[i][j] = pow(omega, (i + startBin) * j);
			}
		}
	}

	void calcDft(paddingType padding){


		//prepare sample window The following parts can be combined
		double tmp_smp_window[window_size];
		for(uint i = 0; i< window_size; i++){
			tmp_smp_window[i] = smp_memory[( i + smp_mem_pos ) % window_size];
		}
		dumpData("/tmp/signal_original",tmp_smp_window,window_size);

		for(uint i = 0; i< window_size; i++){
			tmp_smp_window[i] *= filterWindowCoefficents[i];
		}
		dumpData("/tmp/signal_windowed",tmp_smp_window,window_size);

		dumpData("/tmp/smp_window",smp_memory,window_size);

		for( uint i=0; i < freq_count; i++){
			dftResults[i] = 0;
			for(uint j=0; j < window_size * window_multiplier; j++){
				if(padding == paddingType::ZERO){
					if(j < (window_size)){
						dftResults[i]+= tmp_smp_window[j] * dftMatrix[i][j];
					}else{
						dftResults[i]+= 0;
					}
				}else if(padding == paddingType::SIG_REPEAT){//repeate samples
					dftResults[i]+= tmp_smp_window[j % window_size] * dftMatrix[i][j];
				}			
			}
		}
	}

	void calcWindow(windowType window_type_in){


		if(window_type_in == windowType::FLATTOP){
			for(uint i=0; i < window_size; i++){
				filterWindowCoefficents[i] = 	0.21557895
												- 0.41663158 * cos(2 * M_PI * i / ( window_size ))
												+ 0.277263158 * cos(4 * M_PI * i / ( window_size ))
												- 0.083578947 * cos(6 * M_PI * i / ( window_size ))
												+ 0.006947368 * cos(8 * M_PI * i / ( window_size ));
				window_corretion_factor += filterWindowCoefficents[i];
			}
		}else if(window_type_in == windowType::HAMMING || window_type_in == windowType::HANN){
			double a_0 = 0.5;//this is the hann window
			if(window_type_in == windowType::HAMMING)
				a_0 = 25./46;
			
			for(uint i=0; i < window_size; i++){
				filterWindowCoefficents[i] = 	a_0
												- (1 - a_0) * cos(2 * M_PI * i / ( window_size ));
				window_corretion_factor += filterWindowCoefficents[i];
			}
		}else{
			for(uint i=0; i < window_size; i++){
				filterWindowCoefficents[i] = 1;
				window_corretion_factor += filterWindowCoefficents[i];
			}
		}
		window_corretion_factor /= window_size;
		dumpData("/tmp/filter_window",filterWindowCoefficents,window_size);
	}
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */

