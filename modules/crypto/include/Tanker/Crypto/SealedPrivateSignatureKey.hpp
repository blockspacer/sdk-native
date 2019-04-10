#pragma once

#include <Tanker/Crypto/BasicCryptographicType.hpp>

#include <sodium/crypto_box.h>
#include <sodium/crypto_sign.h>

#include <tuple>
#include <type_traits>

namespace Tanker
{
namespace Crypto
{
class SealedPrivateSignatureKey
  : public BasicCryptographicType<SealedPrivateSignatureKey,
                                  crypto_sign_SECRETKEYBYTES +
                                      crypto_box_SEALBYTES>
{
  using base_t::base_t;
};
}
}

namespace std
{
template <>
class tuple_size<::Tanker::Crypto::SealedPrivateSignatureKey>
  : public integral_constant<size_t,
                             crypto_sign_SECRETKEYBYTES + crypto_box_SEALBYTES>
{
};

template <size_t I>
class tuple_element<I, ::Tanker::Crypto::SealedPrivateSignatureKey>
  : public tuple_element<I, ::Tanker::Crypto::SealedPrivateSignatureKey::base_t>
{
};
}
