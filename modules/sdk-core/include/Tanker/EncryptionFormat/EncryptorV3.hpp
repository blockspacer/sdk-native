#pragma once

#include <Tanker/Crypto/Types.hpp>
#include <Tanker/EncryptionFormat/EncryptionMetadata.hpp>
#include <Tanker/Types/ResourceId.hpp>

#include <gsl-lite.hpp>

#include <cstdint>

namespace Tanker
{
namespace EncryptionFormat
{
namespace EncryptorV3
{
constexpr uint32_t version()
{
  return 3u;
}

uint64_t encryptedSize(uint64_t clearSize);
uint64_t decryptedSize(gsl::span<uint8_t const> encrypteData);
EncryptionMetadata encrypt(uint8_t* encryptedData,
                           gsl::span<uint8_t const> clearData);
void decrypt(uint8_t* decryptedData,
             Crypto::SymmetricKey const& key,
             gsl::span<uint8_t const> encryptedData);
ResourceId extractResourceId(gsl::span<uint8_t const> encryptedData);
}
}
}
