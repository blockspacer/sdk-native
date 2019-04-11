#include <Tanker/Actions/TrustchainCreation.hpp>

#include <Tanker/Crypto/Types.hpp>
#include <Tanker/Serialization/Serialization.hpp>

#include <nlohmann/json.hpp>

#include <tuple>

namespace Tanker
{
Nature TrustchainCreation::nature() const
{
  return Nature::TrustchainCreation;
}

std::vector<Index> TrustchainCreation::makeIndexes() const
{
  return {};
}

bool operator==(TrustchainCreation const& l, TrustchainCreation const& r)
{
  return std::tie(l.publicSignatureKey) == std::tie(r.publicSignatureKey);
}

bool operator!=(TrustchainCreation const& l, TrustchainCreation const& r)
{
  return !(l == r);
}

void from_serialized(Serialization::SerializedSource& ss,
                     TrustchainCreation& tc)
{
  tc.publicSignatureKey =
      Serialization::deserialize<Crypto::PublicSignatureKey>(ss);
}

std::uint8_t* to_serialized(std::uint8_t* it, TrustchainCreation const& tc)
{
  return Serialization::serialize(it, tc.publicSignatureKey);
}

void to_json(nlohmann::json& j, TrustchainCreation const& tc)
{
  j["publicSignatureKey"] = tc.publicSignatureKey;
}
}
