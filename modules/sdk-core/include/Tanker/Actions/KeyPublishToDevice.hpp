#pragma once

#include <Tanker/Crypto/EncryptedSymmetricKey.hpp>
#include <Tanker/Crypto/Mac.hpp>
#include <Tanker/Crypto/Types.hpp>
#include <Tanker/Index.hpp>
#include <Tanker/Nature.hpp>
#include <Tanker/Types/DeviceId.hpp>

#include <gsl-lite.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Tanker
{
struct KeyPublishToDevice
{
  DeviceId recipient;
  Crypto::Mac mac;
  Crypto::EncryptedSymmetricKey key;

  Nature nature() const;
  std::vector<Index> makeIndexes() const;
};

bool operator==(KeyPublishToDevice const& l, KeyPublishToDevice const& r);
bool operator!=(KeyPublishToDevice const& l, KeyPublishToDevice const& r);

KeyPublishToDevice deserializeKeyPublishToDevice(gsl::span<uint8_t const> data);

void to_json(nlohmann::json& j, KeyPublishToDevice const& kp);

std::uint8_t* to_serialized(std::uint8_t* it, KeyPublishToDevice const& kp);

constexpr std::size_t serialized_size(KeyPublishToDevice const& kp)
{
  return DeviceId::arraySize + kp.mac.size() + kp.key.size() +
         Serialization::varint_size(kp.key.size());
}
}
