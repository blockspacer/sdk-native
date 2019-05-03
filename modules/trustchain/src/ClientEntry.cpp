#include <Tanker/Trustchain/ClientEntry.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Serialization/Serialization.hpp>
#include <Tanker/Trustchain/Action.hpp>
#include <Tanker/Trustchain/detail/ComputeHash.hpp>

#include <algorithm>
#include <tuple>

namespace Tanker
{
namespace Trustchain
{
ClientEntry::ClientEntry(TrustchainId const& trustchainId,
                         Crypto::Hash const& author,
                         Actions::Nature nature,
                         std::vector<std::uint8_t> serializedPayload,
                         Crypto::Hash const& hash,
                         Crypto::Signature const& signature)
  : _trustchainId(trustchainId),
    _author(author),
    _nature(nature),
    _serializedPayload(serializedPayload),
    _hash(hash),
    _signature(signature)
{
}

ClientEntry ClientEntry::create(TrustchainId const& trustchainId,
                                Crypto::Hash const& author,
                                Action const& action,
                                Crypto::PrivateSignatureKey const& key)
{
  auto const serializedPayload = Serialization::serialize(action);
  auto const hash =
      detail::computeHash(action.nature(), author, serializedPayload);
  auto const signature = Crypto::sign(hash, key);

  return {trustchainId,
          author,
          action.nature(),
          serializedPayload,
          hash,
          signature};
}

TrustchainId const& ClientEntry::trustchainId() const
{
  return _trustchainId;
}

Crypto::Hash const& ClientEntry::author() const
{
  return _author;
}

Actions::Nature ClientEntry::nature() const
{
  return _nature;
}

std::vector<std::uint8_t> const& ClientEntry::serializedPayload() const
{
  return _serializedPayload;
}

Crypto::Signature const& ClientEntry::signature() const
{
  return _signature;
}

Crypto::Hash const& ClientEntry::hash() const
{
  return _hash;
}

bool operator==(ClientEntry const& lhs, ClientEntry const& rhs)
{
  return lhs.nature() == rhs.nature() &&
         std::tie(lhs.trustchainId(),
                  lhs.author(),
                  lhs.serializedPayload(),
                  lhs.hash(),
                  lhs.signature()) == std::tie(rhs.trustchainId(),
                                               rhs.author(),
                                               rhs.serializedPayload(),
                                               rhs.hash(),
                                               rhs.signature());
}

bool operator!=(ClientEntry const& lhs, ClientEntry const& rhs)
{
  return !(lhs == rhs);
}
}
}
