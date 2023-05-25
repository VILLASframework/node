#include <array>
#include <complex>
#include <stdint.h>
#include <type_traits>
#include <variant>
#include <vector>

namespace villas {
namespace node {
namespace c37_118 {
namespace types {

// cartesian phasor representation
template <typename Format>
struct Rectangular {
	Format real;
	Format imaginary;
};

// polar phasor representation
template <typename Format, bool Unsigned = std::is_integral_v<Format>>
struct Polar;

template <typename Format>
struct Polar<Format, true>
{
	std::make_unsigned_t<Format> magnitude;
	std::make_signed_t<Format> phase;
};

template <typename Format>
struct Polar<Format, false>
{
	Format magnitude;
	Format phase;
};

struct Phasor : public std::variant<Rectangular<int16_t>, Polar<int16_t>, Rectangular<float>, Polar<float>>
{
	std::complex<float> to_complex() const;
};

struct Analog : public std::variant<int16_t, float>
{
	float to_float() const;
};

struct Freq : public std::variant<int16_t, float>
{
};

struct PmuData
{
	uint16_t stat;
	std::vector<Phasor> phasor;
	Freq freq;
	Freq dfreq;
	std::vector<Analog> analog;
	std::vector<uint16_t> digital;
};

struct Data
{
	std::vector<PmuData> pmus;
};

struct Header
{
	std::string data;
};

struct Name1 : public std::string
{
};

struct ChannelInfo
{
	Name1 nam;
	uint32_t unit;
};

struct DigitalInfo
{
	std::array<Name1, 16> nam;
	uint32_t unit;
};

struct PmuConfig1
{
	Name1 stn;
	uint16_t idcode;
	uint16_t format;
	std::vector<ChannelInfo> phinfo;
	std::vector<ChannelInfo> aninfo;
	std::vector<DigitalInfo> dginfo;
	uint16_t fnom;
	uint16_t cfgcnt;
};

struct Config1
{
	uint32_t time_base;
	std::vector<PmuConfig1> pmus;
	uint16_t data_rate;
};

struct Config2 : public Config1 {};

struct Name3 : public std::string
{
};

// ToDo
struct Config3
{
};

struct Command
{
	uint16_t cmd;
	std::vector<unsigned char> ext;

	static constexpr uint16_t DATA_START = 0x1;
	static constexpr uint16_t DATA_STOP = 0x2;
	static constexpr uint16_t GET_HEADER = 0x3;
	static constexpr uint16_t GET_CONFIG1 = 0x4;
	static constexpr uint16_t GET_CONFIG2 = 0x5;
	static constexpr uint16_t GET_CONFIG3 = 0x6;
};

using Message = std::variant<Data, Header, Config1, Config2, Command, Config3>;

struct Frame
{
	uint16_t version;
	uint16_t idcode;
	uint32_t soc;
	uint32_t fracsec;
	Message message;
};

} /* namespace types */
} /* namespace c37_118 */
} /* namespace node */
} /* namespace villas */
