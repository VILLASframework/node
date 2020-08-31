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

#define MULTI 10
#define SMP_RATE 1000

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	struct format_type *format;

	//double* smp_memory;
	double smp_memory[SMP_RATE];

	uint smp_mem_pos;
	uint smp_mem_size;
	
	std::complex<double> dftMatrix[SMP_RATE * MULTI][SMP_RATE * MULTI];
	std::complex<double> omega;
	std::complex<double> M_I;
	std::complex<double> dftResults[SMP_RATE * MULTI];
	double filterWindowCoefficents[SMP_RATE];
	double absDftResults[SMP_RATE * MULTI];

	enum paddingStyle{
		NONE,
		ZERO,
		SIG_REPEAT
	};

public:
	DftHook(struct vpath *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		smp_mem_pos(0),
		smp_mem_size(SMP_RATE),
		M_I(0.0,1.0)
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

		genDftMatrix();
		calcWindow("ha1nning");


		state = State::PREPARED;

	}


	virtual void start()
	{

		assert(state == State::PREPARED || state == State::STOPPED);

		//smp_memory = new double[smp_mem_size];
		if (!smp_memory)
			throw MemoryAllocationError();

		for(uint i = 0; i < smp_mem_size; i++)
			smp_memory[i] = 0;



		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);


		state = State::STOPPED;
	}

	virtual void parse(json_t *cfg)
	{

		assert(state != State::STARTED);

		Hook::parse(cfg);

		state = State::PARSED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		smp_memory[smp_mem_pos % smp_mem_size] = smp->data[1].f;
		smp_mem_pos ++ ;

		if((smp_mem_pos % smp_mem_size) == 0){
			calcDft(paddingStyle::SIG_REPEAT);

			for(uint i=0; i<smp_mem_size * MULTI; i++){
				absDftResults[i] = abs(dftResults[i]);
			}
			info("49.5Hz -> %f\t\t50Hz -> %f\t\t50.5Hz -> %f",absDftResults[99] * 2 / (SMP_RATE * MULTI),absDftResults[100] * 2 / (SMP_RATE * MULTI) ,absDftResults[101] * 2 / (SMP_RATE * MULTI) );
		}
		return Reason::OK;
	}

	virtual ~DftHook()
	{
		//delete smp_memory;
	}


	void genDftMatrix(){
		using namespace std::complex_literals;

		omega = exp((-2 * M_PI * M_I) / (double)(smp_mem_size * MULTI));

		for( uint i=0 ; i < smp_mem_size * MULTI ; i++){
			for( uint j=0 ; j < smp_mem_size * MULTI ; j++){
				dftMatrix[i][j] = pow(omega, i * j);
			}
		}
	}

	void calcDft(paddingStyle padding){
		//prepare sample window
		double tmp_smp_window[SMP_RATE];
		for(uint i = 0; i< smp_mem_size; i++){
			tmp_smp_window[i] = smp_memory[( i + smp_mem_pos + 1) % smp_mem_size] * filterWindowCoefficents[i];
		}


		for( uint i=0; i < smp_mem_size * MULTI; i++){
			dftResults[i] = 0;
			for(uint j=0; j < smp_mem_size * MULTI; j++){
				if(padding == paddingStyle::ZERO){
					if(j < (smp_mem_size)){
						dftResults[i]+= tmp_smp_window[j] * dftMatrix[i][j];
					}else{
						dftResults[i]+= 0;
					}
				}else if(padding == paddingStyle::SIG_REPEAT){//repeate samples
					dftResults[i]+= tmp_smp_window[j % smp_mem_size] * dftMatrix[i][j];
				}			
			}
		}
	}

	void calcWindow(const char *window_name){
		if(strcmp(window_name, "flattop") == 0){
			for(uint i=0; i < smp_mem_size; i++)
				filterWindowCoefficents[i] = 	0.21557895
												- 0.41663158 * cos(2 * M_PI * i / ( smp_mem_size ))
												+ 0.277263158 * cos(4 * M_PI * i / ( smp_mem_size ))
												- 0.083578947 * cos(6 * M_PI * i / ( smp_mem_size ))
												+ 0.006947368 * cos(8 * M_PI * i / ( smp_mem_size ));
		}else if(strcmp(window_name, "hanning") == 0 || strcmp(window_name, "hann") == 0){
			double a_0 = 0.5;//this is the hann window
			if(strcmp(window_name, "hanning"))
				a_0 = 25/46;
			
			for(uint i=0; i < smp_mem_size; i++)
				filterWindowCoefficents[i] = 	a_0
												- (1 - a_0) * cos(2 * M_PI * i / ( smp_mem_size ));
		
		}else{
			for(uint i=0; i < smp_mem_size; i++)
				filterWindowCoefficents[i] = 1;
		}
	}
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */

