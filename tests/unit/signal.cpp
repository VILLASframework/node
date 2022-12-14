/** Unit tests for memory management
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <criterion/criterion.h>

#include <villas/signal.hpp>

using namespace villas::node;

extern void init_memory();

// cppcheck-suppress unknownMacro
Test(signal_data, parse, .init = init_memory) {
	int ret;
	enum SignalType type;
	union SignalData sd;
	const char *str;
	char *end;

	str = "1";
	type = SignalType::INTEGER;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_eq(sd.i, 1);

	str = "1.2";
	type = SignalType::FLOAT;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_float_eq(sd.f, 1.2, 1e-6);

	str = "1";
	type = SignalType::BOOLEAN;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_eq(sd.b, 1);

	str = "1";
	type = SignalType::COMPLEX;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_float_eq(std::real(sd.z), 1, 1e-6);
	cr_assert_float_eq(std::imag(sd.z), 0, 1e-6);

	str = "-1-3i";
	type = SignalType::COMPLEX;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_float_eq(std::real(sd.z), -1, 1e-6);
	cr_assert_float_eq(std::imag(sd.z), -3, 1e-6);

	str = "-3i";
	type = SignalType::COMPLEX;

	ret = sd.parseString(type, str, &end);
	cr_assert_eq(ret, 0);
	cr_assert_eq(end, str + strlen(str));
	cr_assert_float_eq(std::real(sd.z), 0, 1e-6);
	cr_assert_float_eq(std::imag(sd.z), -3, 1e-6);
}
