#pragma once

#include <Tanker/Crypto/BasicCryptographicType.hpp>

#include <sodium/crypto_sign.h>

#include <tuple>
#include <type_traits>

namespace Tanker
{
namespace Crypto
{
extern template class BasicCryptographicType<class Signature,
                                             crypto_sign_BYTES>;

class Signature : public BasicCryptographicType<Signature, crypto_sign_BYTES>
{
  using base_t::base_t;
};
}
}

namespace std
{
template <>
class tuple_size<::Tanker::Crypto::Signature>
  : public integral_constant<size_t, crypto_sign_BYTES>
{
};

template <size_t I>
class tuple_element<I, ::Tanker::Crypto::Signature>
  : public tuple_element<I, ::Tanker::Crypto::Signature::base_t>
{
};
}

#include <Tanker/Crypto/Json/Json.hpp>
#include <Tanker/Crypto/Serialization/Serialization.hpp>
