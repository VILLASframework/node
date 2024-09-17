#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118/types.hpp>
#include <villas/utils.hpp>

using namespace villas::node::c37_118::types;

Phasor::Notation Phasor::notation() const {
  return static_cast<Notation>(variant.index());
}

std::complex<float> Phasor::to_complex() const {
  switch (this->notation()) {
  case RectangularInt: {
    auto [real, imag] = this->get<RectangularInt>();
    return std::complex{float(real), float(imag)};
  }

  case PolarInt: {
    auto [mag, phase] = this->get<PolarInt>();
    return std::polar(float(mag), float(phase));
  }

  case RectangularFloat: {
    auto [real, imag] = this->get<RectangularFloat>();
    return std::complex{real, imag};
  }

  case PolarFloat: {
    auto [mag, phase] = this->get<PolarFloat>();
    return std::polar(mag, phase);
  }

  default:
    throw RuntimeError{"invalid notation"};
  }
}

Phasor Phasor::from_complex(std::complex<float> value, Notation notation) {
  switch (notation) {
  case RectangularInt: {
    return Phasor::make<RectangularInt>(value.real(), value.imag());
  }

  case PolarInt: {
    return Phasor::make<PolarInt>(std::abs(value), std::arg(value));
  }

  case RectangularFloat: {
    return Phasor::make<RectangularFloat>(value.real(), value.imag());
  }

  case PolarFloat: {
    return Phasor::make<PolarFloat>(std::abs(value), std::arg(value));
  }

  default:
    throw RuntimeError{"invalid notation"};
  }
}

Number::Notation Number::notation() const {
  return static_cast<Notation>(variant.index());
}

float Number::to_float() const {
  switch (this->notation()) {
  case Int: {
    return this->get<Int>();
  }

  case Float: {
    return this->get<Float>();
  }

  default:
    throw RuntimeError{"invalid notation"};
  }
}

Number Number::from_float(float value, Notation notation) {
  switch (notation) {
  case Int: {
    return Number::make<Int>(value);
  }

  case Float: {
    return Number::make<Float>(value);
  }

  default:
    throw RuntimeError{"invalid notation"};
  }
}
