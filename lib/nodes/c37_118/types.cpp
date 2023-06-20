#include <villas/utils.hpp>
#include <villas/nodes/c37_118/types.hpp>

using namespace villas::node::c37_118::types;

std::complex<float> Phasor::to_complex() const
{
	return std::visit(villas::utils::overloaded {
		[](Polar<int16_t> phasor){
			return std::polar<float>(phasor.magnitude, phasor.phase / 1000.);
		},
		[](Polar<float> phasor){
			return std::polar<float>(phasor.magnitude, phasor.phase);
		},
		[](auto phasor){
			return std::complex<float>(phasor.real, phasor.imaginary);
		},
	}, *this);
}

float Analog::to_float() const {
	return std::visit(villas::utils::overloaded {
		[](int16_t analog){
			return (float) analog;
		},
		[](float analog){
			return analog;
		},
	}, *this);
}

