#pragma once

#include <Tanker/Crypto/AsymmetricKey.hpp>
#include <Tanker/Crypto/IsCryptographicType.hpp>
#include <Tanker/Crypto/KeyType.hpp>
#include <Tanker/Crypto/KeyUsage.hpp>

#include <Tanker/Crypto/detail/CryptographicTypeImpl.hpp>

#include <sodium/crypto_sign.h>

#include <array>
#include <cstdint>
#include <type_traits>

namespace Tanker
{
namespace Crypto
{
template <>
class AsymmetricKey<KeyType::Private, KeyUsage::Signature>
  : std::array<std::uint8_t, crypto_sign_SECRETKEYBYTES>
{
  TANKER_CRYPTO_CRYPTOGRAPHIC_TYPE_IMPL(AsymmetricKey,
                                        crypto_sign_SECRETKEYBYTES,
                                        PrivateSignatureKey)
};

using PrivateSignatureKey =
    AsymmetricKey<KeyType::Private, KeyUsage::Signature>;

template <>
struct IsCryptographicType<PrivateSignatureKey> : std::true_type
{
};
}
}
