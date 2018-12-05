#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>

#include <Tanker/Serialization/SerializedSource.hpp>
#include <Tanker/Serialization/from_serialized.hpp>
#include <Tanker/Serialization/serialized_size.hpp>
#include <Tanker/Serialization/to_serialized.hpp>

namespace Tanker
{
namespace Serialization
{
template <typename T>
std::vector<std::uint8_t> serialize(T const& val)
{
  std::vector<std::uint8_t> buffer;
  auto const size = serialized_size(val);
  buffer.reserve(size);
  to_serialized(std::back_inserter(buffer), val);
  assert(buffer.capacity() == size);
  assert(buffer.size() == size);
  return buffer;
}

template <typename OutputIterator, typename T>
void serialize(OutputIterator it, T const& val)
{
  to_serialized(it, val);
}

template <typename T>
void deserialize(SerializedSource& ss, T& val)
{
  detail::deserialize_impl(ss, val);
}

template <typename T,
          typename = std::enable_if_t<std::is_default_constructible<T>::value>>
T deserialize(SerializedSource& ss)
{
  T ret;
  deserialize(ss, ret);
  return ret;
}

template <typename T,
          typename = std::enable_if_t<std::is_default_constructible<T>::value>>
void deserialize(gsl::span<std::uint8_t const> serialized, T& val)
{
  SerializedSource ss{serialized};

  deserialize(ss, val);
  if (!ss.eof())
    throw std::runtime_error("deserialize: some input left");
}

template <typename T,
          typename = std::enable_if_t<std::is_default_constructible<T>::value>>
T deserialize(gsl::span<std::uint8_t const> serialized)
{
  T ret;
  deserialize(serialized, ret);
  return ret;
}
}
}
