/**
 * @file
 * @author Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <optional>
#include <cstring>
#include <villas/nodes/c37_118/types.hpp>

namespace villas {
namespace node {
namespace c37_118 {
namespace parser {

using Result = std::intptr_t;

enum class Status : Result {
	Ok = 0,
	MissingBytes,
	MissingConfig,
	InvalidValue,
	InvalidChecksum,
	InvalidSlice,
	Other,
};

template <typename T = Result>
Result ok(T t = 0);
Result error(Status);
Status status(Result);
bool is_ok(Result);

// context used to assemble and disassemble data frames with the information contained in config structs
class Context {
public:
	using Config = std::variant<types::Config2 /*, struct Config3 */>;
	Config config;

	Context(Config config);
	void reset();
	void next_pmu();
	uint16_t num_pmu() const;
	uint16_t format() const;
	uint16_t phnmr() const;
	uint16_t annmr() const;
	uint16_t dgnmr() const;

private:
	size_t pmu_index;
};

// placeholder type allowing deferred assignment to a field during serialization
template <typename T>
class Placeholder;

class Parser {
public:
	std::optional<Context> context;

	using ssize_t = std::ptrdiff_t;

	Parser() = default;
	Parser(unsigned char *buffer, size_t length, std::optional<Context::Config> config = std::nullopt);
	Result slice(ssize_t from, size_t length);
	Result require(size_t length);
	Parser subparser(unsigned char *buffer, size_t length);
	size_t remaining() const;

	template <typename T>
	Result copy_from(T const *arg, size_t length = sizeof(T));

	template <typename T>
	Result copy_to(T *arg, size_t length = sizeof(T));

	template <typename Arg1, typename Arg2, typename... Args>
	Result deserialize(Arg1 *arg1, Arg2 *arg2, Args *...args);

	Result deserialize(uint16_t *i);
	Result deserialize(uint32_t *i);
	Result deserialize(int16_t *i);
	Result deserialize(int32_t *i);
	Result deserialize(float *f);

	template <typename T>
	Result deserialize(std::vector<T> *v);

	template <typename T, size_t N>
	Result deserialize(std::array<T, N> *a);

	template <typename Format>
	Result deserialize(types::Rectangular<Format> *r);

	template <typename Format>
	Result deserialize(types::Polar<Format> *r);

	Result deserialize(types::Phasor *p);
	Result deserialize(types::Analog *a);
	Result deserialize(types::Freq *f);
	Result deserialize(types::PmuData *d);
	Result deserialize(types::Data *d);
	Result deserialize(types::Header *h);
	Result deserialize(types::Name1 *n);
	Result deserialize(types::ChannelInfo *c);
	Result deserialize(types::DigitalInfo *d);
	Result deserialize(types::PmuConfig1 *c);
	Result deserialize(types::Config1 *c);
	Result deserialize(types::Config2 *c);
	Result deserialize(types::Name3 *n);
	Result deserialize(types::Config3 *c);
	Result deserialize(types::Command *c);
	Result deserialize(types::Frame *f);

	template <typename Arg1, typename Arg2, typename... Args>
	Result serialize(Arg1 const *arg1, Arg2 const *arg2, Args const *...args);
	Result serialize(uint8_t const *i);
	Result serialize(uint16_t const *i);
	Result serialize(uint32_t const *i);
	Result serialize(int8_t const *i);
	Result serialize(int16_t const *i);
	Result serialize(int32_t const *i);
	Result serialize(float const *f);

	template <typename T>
	Result serialize(Placeholder<T> const *p);

	template <typename T>
	Result serialize(std::vector<T> const *v);

	template <typename T, size_t N>
	Result serialize(std::array<T, N> const *a);

	template <typename Format>
	Result serialize(types::Rectangular<Format> const *r);

	template <typename Format>
	Result serialize(types::Polar<Format> const *r);

	Result serialize(types::Phasor const *p);
	Result serialize(types::Analog const *a);
	Result serialize(types::Freq const *f);
	Result serialize(types::PmuData const *d);
	Result serialize(types::Data const *d);
	Result serialize(types::Header const *h);
	Result serialize(types::Name1 const *n);
	Result serialize(types::ChannelInfo const *c);
	Result serialize(types::DigitalInfo const *d);
	Result serialize(types::PmuConfig1 const *c);
	Result serialize(types::Config1 const *c);
	Result serialize(types::Config2 const *c);
	Result serialize(types::Name3 const *n);
	Result serialize(types::Config3 const *c);
	Result serialize(types::Command const *c);
	Result serialize(types::Frame const *f);

private:
	unsigned char *start, *cursor, *end;
};

// placeholder type allowing deferred assignment to a field during serialization
template <typename T>
class Placeholder
{
public:
	Placeholder() = default;
	Result replace(T *t);
private:
	mutable std::optional<Parser> saved_parser;
	friend Result Parser::serialize(Placeholder<T> const *p);
};

uint16_t calculate_crc(unsigned char *frame, uint16_t size);

} /* namespace parser */
} /* namespace c37_118 */
} /* namespace node */
} /* namespace villas */
