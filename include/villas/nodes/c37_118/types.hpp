#include <array>
#include <complex>
#include <optional>
#include <stdint.h>
#include <type_traits>
#include <variant>
#include <vector>

namespace villas::node::c37_118::types {

class Phasor final {
public:
  enum Notation {
    RectangularInt = 0,
    PolarInt,
    RectangularFloat,
    PolarFloat,
  };
  using RectangularInt_t = std::tuple<int16_t, int16_t>;
  using PolarInt_t = std::tuple<uint16_t, int16_t>;
  using RectangularFloat_t = std::tuple<float, float>;
  using PolarFloat_t = std::tuple<float, float>;
  using Variant = std::variant<RectangularInt_t, PolarInt_t, RectangularFloat_t,
                               PolarFloat_t>;

  Phasor() = default;

  Notation notation() const;
  std::complex<float> to_complex() const;
  static Phasor from_complex(std::complex<float> value, Notation notation);

  template <Notation N, typename Tuple = std::variant_alternative_t<N, Variant>,
            typename First = std::tuple_element<0, Tuple>,
            typename Second = std::tuple_element<1, Tuple>>
  static Phasor make(First first, Second second);

  template <Notation N, typename Tuple = std::variant_alternative_t<N, Variant>>
  Tuple get() const;

private:
  Phasor(Variant variant) : variant{std::move(variant)} {}
  Variant variant;
};

template <Phasor::Notation N, typename Tuple, typename First, typename Second>
Phasor Phasor::make(First first, Second second) {
  return Variant{std::in_place_index<N>, first, second};
}

template <Phasor::Notation N, typename Tuple> Tuple Phasor::get() const {
  return std::get<N>(this->variant);
}

class Number final {
public:
  enum Notation {
    Int,
    Float,
  };
  using Int_t = int16_t;
  using Float_t = float;
  using Variant = std::variant<Int_t, Float_t>;

  Number() = default;

  Notation notation() const;
  float to_float() const;
  static Number from_float(float value, Notation notation);

  template <Notation N, typename Value = std::variant_alternative_t<N, Variant>>
  static Number make(Value value);

  template <Notation N, typename Value = std::variant_alternative_t<N, Variant>>
  Value get() const;

private:
  Number(Variant variant) : variant{std::move(variant)} {}
  Variant variant;
};

using Analog = Number;
using Freq = Number;

struct PmuData final {
  uint16_t stat;
  std::vector<Phasor> phasor;
  Freq freq;
  Freq dfreq;
  std::vector<Analog> analog;
  std::vector<uint16_t> digital;
};

template <Number::Notation N, typename Value> Number Number::make(Value value) {
  return Variant{std::in_place_index<N>, value};
}

template <Number::Notation N, typename Value> Value Number::get() const {
  return std::get<N>(this->variant);
}

struct Data final {
  std::vector<PmuData> pmus;
};

struct Header final {
  std::string data;
};

struct ChannelInfo final {
  std::string nam;
  uint32_t unit;
};

struct DigitalInfo final {
  std::array<std::string, 16> nam;
  uint32_t unit;
};

struct PmuConfig final {
  std::string stn;
  uint16_t idcode;
  uint16_t format;
  std::vector<ChannelInfo> phinfo;
  std::vector<ChannelInfo> aninfo;
  std::vector<DigitalInfo> dginfo;
  uint16_t fnom;
  uint16_t cfgcnt;
};

struct Config {
  uint32_t time_base;
  std::vector<PmuConfig> pmus;
  uint16_t data_rate;
};

class Config1 {
private:
  Config inner;

public:
  Config1() = delete;
  Config1(Config config) noexcept : inner(config) {}
  operator Config&() noexcept {return inner;}
  operator Config const&() const noexcept {return inner;}
  Config* operator ->() { return &inner; }
  Config const* operator ->() const { return &inner; }
};

class Config2 {
private:
  Config inner;

public:
  Config2() = delete;
  Config2(Config config) noexcept : inner(config) {}
  operator Config&() noexcept {return inner;}
  operator Config const&() const noexcept {return inner;}
  Config* operator ->() { return &inner; }
  Config const* operator ->() const { return &inner; }
};

struct Command final {
  uint16_t cmd;
  std::vector<unsigned char> ext;

  static constexpr uint16_t DATA_START = 0x1;
  static constexpr uint16_t DATA_STOP = 0x2;
  static constexpr uint16_t GET_HEADER = 0x3;
  static constexpr uint16_t GET_CONFIG1 = 0x4;
  static constexpr uint16_t GET_CONFIG2 = 0x5;
  //static constexpr uint16_t GET_CONFIG3 = 0x6;
};

struct Frame final {
  using Variant = std::variant<Data, Header, Config1, Config2, Command>;

  uint16_t version;
  uint16_t framesize;
  uint16_t idcode;
  uint32_t soc;
  uint32_t fracsec;
  Variant message;
};

} // namespace villas::node::c37_118::types
