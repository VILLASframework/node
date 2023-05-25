#include <arpa/inet.h>
#include <villas/utils.hpp>
#include <villas/nodes/c37_118/parser.hpp>

using namespace villas::node::c37_118;
using namespace villas::node::c37_118::parser;
using namespace villas::node::c37_118::types;

uint16_t parser::calculate_crc(unsigned char *frame, uint16_t size)
{
	uint16_t crc = 0xFFFF;
	uint16_t temp;
	uint16_t quick;

	for(int i = 0; i < size; i++) {
		temp = (crc>>8) ^ (uint16_t) frame[i];
		crc <<= 8;
		quick = temp ^ (temp >> 4);
		crc ^= quick;
		quick <<= 5;
		crc ^= quick;
		quick <<= 7;
		crc ^= quick;
	}

	return crc;
}

Context::Context(Config config) :
	config(config),
	pmu_index(0)
{}

void Context::reset()
{
	pmu_index = 0;
}

void Context::next_pmu()
{
	pmu_index++;
}

uint16_t Context::num_pmu() const
{
	return std::visit([](auto &c){
		return c.pmus.size();
	}, config);
}

uint16_t Context::format() const
{
	return std::visit([&](auto &c){
		return c.pmus[pmu_index].format;
	}, config);
}

uint16_t Context::phnmr() const
{
	return std::visit([&](auto &c){
		return c.pmus[pmu_index].phinfo.size();
	}, config);
}

uint16_t Context::annmr() const
{
	return std::visit([&](auto &c){
		return c.pmus[pmu_index].aninfo.size();
	}, config);
}

uint16_t Context::dgnmr() const
{
	return std::visit([&](auto &c){
		return c.pmus[pmu_index].dginfo.size();
	}, config);
}

bool parser::is_ok(Result result)
{
	return result >= 0;
}

template <typename T>
Result parser::ok(T t)
{
	return (intptr_t) t;
}

Result parser::error(Status s)
{
	return -static_cast<Result>(s);
}

Status parser::status(Result s)
{
	if (s >= -static_cast<Result>(Status::Ok))
		return Status::Ok;
	if (s <= -static_cast<Result>(Status::Other))
		return Status::Other;
	return static_cast<Status>(-s);
}

Parser::Parser(unsigned char *buffer, size_t length, std::optional<Context::Config> config) :
	start(buffer),
	cursor(buffer),
	end(buffer + length)
{
	if (config)
		context.emplace(*config);
}

Result Parser::slice(ssize_t from, size_t length)
{
	if (cursor + from < start)
		return error(Status::InvalidSlice);
	if (cursor + from + length > end)
		return error(Status::MissingBytes);
	return ok(cursor + from);
}

Result Parser::require(size_t length)
{
	auto old = cursor;
	if (end - cursor < (ssize_t) length) return error(Status::MissingBytes);
	cursor += length;
	return ok(old);
}

Parser Parser::subparser(unsigned char *buffer, size_t length)
{
	return Parser {
		buffer,
		length,
		context ? std::optional { context->config } : std::nullopt,
	};
}

size_t Parser::remaining() const
{
	return end - cursor;
}

template <typename T>
Result Parser::copy_from(T const *arg, size_t length)
{
	auto result = require(length);
	if (!is_ok(result)) return result;
	std::memcpy((void *)result, arg, length);
	return length;
}

template <typename T>
Result Parser::copy_to(T *arg, size_t length)
{
	auto result = require(length);
	if (!is_ok(result)) return result;
	std::memcpy(arg, (void const*)result, length);
	return length;
}

template <typename Arg1, typename Arg2, typename... Args>
Result Parser::deserialize(Arg1 *arg1, Arg2 *arg2, Args *...args)
{
	auto head = deserialize(arg1);
	if (!is_ok(head)) return head;
	auto tail = deserialize(arg2, args...);
	if (!is_ok(tail)) return tail;
	return head + tail;
}

Result Parser::deserialize(uint16_t *i)
{
	uint16_t big_endian;
	auto result = copy_to(&big_endian);
	if (is_ok(result))
		*i = ntohs(big_endian);
	return result;
}

Result Parser::deserialize(uint32_t *i)
{
	uint32_t big_endian;
	auto result = copy_to(&big_endian);
	if (is_ok(result))
		*i = ntohl(big_endian);
	return result;
}

Result Parser::deserialize(int16_t *i)
{
	return deserialize((uint16_t *)i);
}

Result Parser::deserialize(int32_t *i)
{
	return deserialize((uint32_t *)i);
}

Result Parser::deserialize(float *f)
{
	return deserialize((uint32_t *)f);
}

template <typename Arg1, typename Arg2, typename... Args>
Result Parser::serialize(Arg1 const *arg1, Arg2 const *arg2, Args const *...args)
{
	auto head = serialize(arg1);
	if (!is_ok(head)) return head;
	auto tail = serialize(arg2, args...);
	if (!is_ok(tail)) return tail;
	return head + tail;
}

Result Parser::serialize(uint8_t const *i)
{
	return copy_from(i);
}

Result Parser::serialize(uint16_t const *i)
{
	uint16_t big_endian = htons(*i);
	return copy_from(&big_endian);
}

Result Parser::serialize(uint32_t const *i)
{
	uint32_t big_endian = htonl(*i);
	return copy_from(&big_endian);
}

Result Parser::serialize(int8_t const *i)
{
	return serialize((uint8_t const *)i);
}

Result Parser::serialize(int16_t const *i)
{
	return serialize((uint16_t const *)i);
}

Result Parser::serialize(int32_t const *i)
{
	return serialize((uint32_t const *)i);
}

Result Parser::serialize(float const *f)
{
	return serialize((uint32_t const *)f);
}

template<class T, size_t N>
Result Parser::serialize(std::array<T, N> const *a)
{
	for (auto const &elem : *a) {
		auto result = serialize(&elem);
		if (!is_ok(result)) return result;
	}
	return ok();
}

template<class T, long unsigned int N>
Result Parser::deserialize(std::array<T, N> *a)
{
	for (auto &elem : *a) {
		auto result = deserialize(&elem);
		if (!is_ok(result)) return result;
	}
	return ok();
}

template<class T>
Result Parser::serialize(std::vector<T> const *v)
{
	for (auto const &elem : *v) {
		auto result = serialize(&elem);
		if (!is_ok(result)) return result;
	}
	return ok();
}

template<class T>
Result Parser::deserialize(std::vector<T> *v)
{
	for (auto &elem : *v) {
		auto result = deserialize(&elem);
		if (!is_ok(result)) return result;
	}
	return ok();
}

Result Parser::deserialize(Phasor *p)
{
	switch (context.value().format() & 0x3) {
		case 0x0:
			p->emplace<0>();
			break;
		case 0x1:
			p->emplace<1>();
			break;
		case 0x2:
			p->emplace<2>();
			break;
		case 0x3:
			p->emplace<3>();
			break;
	}
	return std::visit([&](auto &phasor){
		return deserialize(&phasor);
	}, *p);
}

Result Parser::serialize(Phasor const *p)
{
	if ((context.value().format() & 0x3) != p->index())
		return error(Status::InvalidValue);
	return std::visit([&](auto &phasor){
		return serialize(&phasor);
	}, *p);
}

template <typename T>
Result Parser::serialize(Rectangular<T> const *r)
{
	return serialize(&r->real, &r->imaginary);
}

template <typename T>
Result Parser::deserialize(Rectangular<T> *r)
{
	return deserialize(&r->real, &r->imaginary);
}

template <typename T>
Result Parser::serialize(Polar<T> const *p)
{
	return serialize(&p->magnitude, &p->phase);
}

template <typename T>
Result Parser::deserialize(Polar<T> *p)
{
	return deserialize(&p->magnitude, &p->phase);
}

Result Parser::deserialize(Analog *a)
{
	if (context.value().format() & 0x4)
		a->emplace<1>();
	else
		a->emplace<0>();
	return std::visit([&](auto &analog){
		return deserialize(&analog);
	}, *a);
}

Result Parser::serialize(Analog const *a)
{
	if ((context.value().format() & 0x4) != (a->index() << 2))
		return error(Status::InvalidValue);
	return std::visit([&](auto &analog){
		return serialize(&analog);
	}, *a);
}

Result Parser::deserialize(Freq *f)
{
	if (context.value().format() & 0x8)
		f->emplace<1>();
	else
		f->emplace<0>();
	return std::visit([&](auto &freq){
		return deserialize(&freq);
	}, *f);
}

Result Parser::serialize(Freq const *f)
{
	if ((context.value().format() & 0x8) != (f->index() << 3))
		return error(Status::InvalidValue);
	return std::visit([&](auto &freq){
		return serialize(&freq);
	}, *f);
}

Result Parser::deserialize(PmuData *p)
{
	auto &ctx = context.value();
	p->phasor.resize(ctx.phnmr());
	p->analog.resize(ctx.annmr());
	p->digital.resize(ctx.dgnmr());
	auto result = deserialize(&p->stat, &p->phasor, &p->freq, &p->dfreq, &p->analog, &p->digital);
	ctx.next_pmu();
	return result;
}

Result Parser::serialize(PmuData const *p)
{
	auto &ctx = context.value();
	if (p->phasor.size() != ctx.phnmr() ||
		p->analog.size() != ctx.annmr() ||
		p->digital.size() != ctx.dgnmr())
		return error(Status::InvalidValue);
	auto result = serialize(&p->stat, &p->phasor, &p->freq, &p->dfreq, &p->analog, &p->digital);
	ctx.next_pmu();
	return result;
}

Result Parser::deserialize(Data *d)
{
	if (!context)
		return error(Status::MissingConfig);
	d->pmus.resize(context->num_pmu());
	context->reset();
	return deserialize(&d->pmus);
}

Result Parser::serialize(Data const *d)
{
	if (!context)
		return error(Status::MissingConfig);
	if (d->pmus.size() != context->num_pmu())
		return error(Status::InvalidValue);
	context->reset();
	return serialize(&d->pmus);
}

Result Parser::deserialize(Header *h)
{
	h->data.resize(remaining());
	return copy_to(h->data.data(), h->data.size());
}

Result Parser::serialize(Header const *h)
{
	return copy_from(h->data.data(), h->data.size());
}

Result Parser::deserialize(Name1 *n)
{
	n->resize(16);
	return copy_to(n->data(), n->size());
}

Result Parser::serialize(Name1 const *n)
{
	auto buffer = std::array<char, 16> {};
	std::memcpy(buffer.data(), n->data(), std::min(n->size(), buffer.size()));
	return copy_from(&buffer);
}


Result Parser::deserialize(PmuConfig1 *p)
{
	uint16_t phnmr;
	uint16_t annmr;
	uint16_t dgnmr;
	auto result = deserialize(
		&p->stn,
		&p->idcode,
		&p->format,
		&phnmr,
		&annmr,
		&dgnmr
	);
	if (!is_ok(result)) return result;

	auto phnam = std::vector<Name1> {};
	auto annam = std::vector<Name1> {};
	auto dgnam = std::vector<std::array<Name1, 16>> {};
	auto phunit = std::vector<uint32_t> {};
	auto anunit = std::vector<uint32_t> {};
	auto dgunit = std::vector<uint32_t> {};
	phnam.resize(phnmr);
	annam.resize(annmr);
	dgnam.resize(dgnmr);
	phunit.resize(phnmr);
	anunit.resize(annmr);
	dgunit.resize(dgnmr);
	result = deserialize(
		&phnam,
		&annam,
		&dgnam,
		&phunit,
		&anunit,
		&dgunit,
		&p->fnom,
		&p->cfgcnt
	);
	if (!is_ok(result)) return result;

	p->phinfo.resize(phnmr);
	for (size_t i = 0; i < phnmr; i++)
		p->phinfo[i] = ChannelInfo { phnam[i], phunit[i] };

	p->aninfo.resize(annmr);
	for (size_t i = 0; i < annmr; i++)
		p->aninfo[i] = ChannelInfo { annam[i], anunit[i] };

	p->dginfo.resize(dgnmr);
	for (size_t i = 0; i < dgnmr; i++)
		p->dginfo[i] = DigitalInfo { dgnam[i], dgunit[i] };

	return ok();
}

Result Parser::serialize(PmuConfig1 const *p)
{
	uint16_t phnmr = p->phinfo.size();
	uint16_t annmr = p->aninfo.size();
	uint16_t dgnmr = p->dginfo.size();
	auto phnam = std::vector<Name1> {};
	auto annam = std::vector<Name1> {};
	auto dgnam = std::vector<std::array<Name1, 16>> {};
	auto phunit = std::vector<uint32_t> {};
	auto anunit = std::vector<uint32_t> {};
	auto dgunit = std::vector<uint32_t> {};
	phnam.resize(phnmr);
	annam.resize(annmr);
	dgnam.resize(dgnmr);
	phunit.resize(phnmr);
	anunit.resize(annmr);
	dgunit.resize(dgnmr);
	for (size_t i = 0; i < phnmr; i++) {
		phnam[i] = p->phinfo[i].nam;
		phunit[i] = p->phinfo[i].unit;
	}
	for (size_t i = 0; i < annmr; i++) {
		annam[i] = p->aninfo[i].nam;
		anunit[i] = p->aninfo[i].unit;
	}
	for (size_t i = 0; i < dgnmr; i++) {
		dgnam[i] = p->dginfo[i].nam;
		dgunit[i] = p->dginfo[i].unit;
	}
	return serialize(
		&p->stn,
		&p->idcode,
		&p->format,
		&phnmr,
		&annmr,
		&dgnmr,
		&phnam,
		&annam,
		&dgnam,
		&phunit,
		&anunit,
		&dgunit,
		&p->fnom,
		&p->cfgcnt
	);
}

Result Parser::deserialize(Config1 *c)
{
	uint16_t num_pmu;
	auto result = deserialize(
		&c->time_base,
		&num_pmu
	);
	if (!is_ok(result)) return result;

	c->pmus.resize(num_pmu);
	return deserialize(
		&c->pmus,
		&c->data_rate
	);
}

Result Parser::serialize(Config1 const *c)
{
	uint16_t num_pmu = c->pmus.size();
	return serialize(
		&c->time_base,
		&num_pmu,
		&c->pmus,
		&c->data_rate
	);
}

Result Parser::deserialize(Config2 *c)
{
	return deserialize((Config1 *)c);
}

Result Parser::serialize(Config2 const *c)
{
	return serialize((Config1 const *)c);
}

Result Parser::deserialize(Name3 *n)
{
	uint8_t length = 0;
	auto result = copy_from(&length);
	if (!is_ok(result)) return result;
	if (length != 0) {
		n->resize(length);
		return copy_from(n->data(), n->size());
	} else {
		n->clear();
		return ok();
	}
}

Result Parser::serialize(Name3 const *n)
{
	auto length = (unsigned char)std::min(n->size(), (size_t)255);
	auto result = serialize(&length);
	if (!is_ok(result) || length == 0) return result;
	return copy_from(n->data(), length);
}

Result Parser::deserialize(Config3 *c)
{
	assert("unimplemented");
	return ok();
}

Result Parser::serialize(Config3 const *c)
{
	assert("unimplemented");
	return ok();
}

Result Parser::deserialize(Command *c)
{
	auto result = deserialize(&c->cmd);
	if (!is_ok(result)) return result;
	c->ext.resize(remaining());
	return copy_to(c->ext.data(), c->ext.size());
}

Result Parser::serialize(Command const *c)
{
	auto result = serialize(&c->cmd);
	if (!is_ok(result)) return result;
	return copy_from(c->ext.data(), c->ext.size());
}

Result Parser::deserialize(Frame *f)
{
	uint16_t sync;
	uint16_t framesize;
	auto result = deserialize(
		&sync,
		&framesize,
		&f->idcode,
		&f->soc,
		&f->fracsec
	);
	if (!is_ok(result)) return result;

	auto contentsize = framesize - sizeof(uint16_t);
	auto messagesize = contentsize - result;

	result = slice(-result, contentsize);
	if (!is_ok(result)) return result;

	uint16_t calculated_crc = calculate_crc((unsigned char *)result, contentsize);

	if ((sync & 0xFF00) != 0xAA00)
		return error(Status::InvalidValue);
	f->version = sync & 0xF;
	switch (sync & 0xF0) {
		case 0x00:
			f->message = Data {};
			break;
		case 0x10:
			f->message = Header {};
			break;
		case 0x20:
			f->message = Config1 {};
			break;
		case 0x30:
			f->message = Config2 {};
			break;
		case 0x40:
			f->message = Command {};
			break;
		case 0x50:
			f->message = Config3 {};
			break;
		default:
			return error(Status::InvalidValue);
	}

	auto message_parser = subparser(
		(unsigned char *)require(messagesize),
		messagesize
	);
	result = std::visit([&](auto &message) {
		return message_parser.deserialize(&message);
	}, f->message);
	if (!is_ok(result)) return result;

	uint16_t crc;
	result = deserialize(&crc);
	if (!is_ok(result)) return result;

	if (crc != calculated_crc)
		return error(Status::InvalidChecksum);

	return ok();
}

Result Parser::serialize(Frame const *f)
{
	uint16_t type;
	switch (f->message.index()) {
		case 0:
			type = 0x00;
			break;
		case 1:
			type = 0x10;
			break;
		case 2:
			type = 0x20;
			break;
		case 3:
			type = 0x30;
			break;
		case 4:
			type = 0x50;
			break;
		case 5:
			type = 0x40;
			break;
		default:
			return error(Status::InvalidValue);
	}

	uint16_t sync = 0xAA00 | type | f->version;
	Placeholder<uint16_t> framesize;
	auto result = serialize(
		&sync,
		&framesize,
		&f->idcode,
		&f->soc,
		&f->fracsec
	);
	if (!is_ok(result)) return result;
	ptrdiff_t size = result;

	result = std::visit([&](auto message) {
		return serialize(&message);
	}, f->message);
	if (!is_ok(result)) return result;
	size += result;

	uint16_t total = size + sizeof(uint16_t);
	framesize.replace(&total);

	uint16_t crc = calculate_crc((unsigned char *)slice(-(Parser::ssize_t)size, size), size);
	result = serialize(&crc);
	if (!is_ok(result)) return result;

	return ok();
}

template <typename T>
Result Placeholder<T>::replace(T *t)
{
	return saved_parser.value().serialize(t);
}

template <typename T>
Result Parser::serialize(Placeholder<T> const *p)
{
	p->saved_parser = *this;
	T t = {};
	auto result = serialize(&t);
	if (is_ok(result))
		// Restrict the subparser to the length of the default value.
		// Trying to serialize a value which exceeds the size of the default value at a later time,
		// returns an error.
		p->saved_parser = p->saved_parser->subparser((unsigned char *)p->saved_parser->require(result), result);
	else
		p->saved_parser.reset();
	return result;
}
