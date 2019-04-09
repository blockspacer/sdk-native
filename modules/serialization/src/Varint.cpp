#include <Tanker/Serialization/Varint.hpp>

namespace Tanker
{
namespace Serialization
{
std::pair<std::size_t, gsl::span<uint8_t const>> varint_read(
    gsl::span<uint8_t const> data)
{
  std::size_t value = 0;
  std::size_t factor = 1;
  while ((data.at(0) & 0x80) != 0)
  {
    value += (data.at(0) & 0x7f) * factor;
    factor *= 128;
    data = data.subspan(1);
  }
  value += data.at(0) * factor;
  data = data.subspan(1);
  return {value, data};
}

std::uint8_t* varint_write(std::uint8_t* it, std::size_t value)
{
  while (value > 127)
  {
    *it++ = (0x80 | value);
    value /= 128;
  }
  *it++ = value;
  return it;
}
}
}
