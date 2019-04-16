#pragma once

#include <Tanker/Actions/UserKeyPair.hpp>
#include <Tanker/Crypto/EncryptionKeyPair.hpp>
#include <Tanker/Crypto/PublicSignatureKey.hpp>
#include <Tanker/Crypto/Signature.hpp>
#include <Tanker/Index.hpp>
#include <Tanker/Serialization/SerializedSource.hpp>
#include <Tanker/Trustchain/Actions/Nature.hpp>
#include <Tanker/Trustchain/UserId.hpp>

#include <gsl-lite.hpp>
#include <mpark/variant.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional.hpp>

#include <cstddef>
#include <vector>

namespace Tanker
{
namespace Identity
{
struct Delegation;
}

namespace detail
{
template <typename T>
constexpr std::size_t sizeOfCommonDeviceCreationFields(T const& dc)
{
  return dc.ephemeralPublicSignatureKey.arraySize + dc.userId.arraySize +
         dc.delegationSignature.arraySize + dc.publicSignatureKey.arraySize +
         dc.publicEncryptionKey.arraySize;
}
}

struct DeviceCreation1
{
  static constexpr auto const nature =
      Trustchain::Actions::Nature::DeviceCreation;

  Crypto::PublicSignatureKey ephemeralPublicSignatureKey;
  Trustchain::UserId userId;
  Crypto::Signature delegationSignature;
  Crypto::PublicSignatureKey publicSignatureKey;
  Crypto::PublicEncryptionKey publicEncryptionKey;
};

struct DeviceCreation3
{
  static constexpr auto const nature =
      Trustchain::Actions::Nature::DeviceCreation3;

  Crypto::PublicSignatureKey ephemeralPublicSignatureKey;
  Trustchain::UserId userId;
  Crypto::Signature delegationSignature;
  Crypto::PublicSignatureKey publicSignatureKey;
  Crypto::PublicEncryptionKey publicEncryptionKey;
  UserKeyPair userKeyPair;
  bool isGhostDevice;
};

class DeviceCreation
{
public:
  using variant_type = mpark::variant<DeviceCreation1, DeviceCreation3>;

  explicit DeviceCreation(variant_type&&);
  explicit DeviceCreation(variant_type const&);

  DeviceCreation& operator=(variant_type&&);
  DeviceCreation& operator=(variant_type const&);

  DeviceCreation() = default;
  DeviceCreation(DeviceCreation const&) = default;
  DeviceCreation(DeviceCreation&&) = default;
  DeviceCreation& operator=(DeviceCreation const&) = default;
  DeviceCreation& operator=(DeviceCreation&&) = default;

  static DeviceCreation createV1(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey);

  static DeviceCreation createV3(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey,
      bool isGhostDevice);

  variant_type const& variant() const;

  Trustchain::Actions::Nature nature() const;
  Crypto::PublicSignatureKey const& ephemeralPublicSignatureKey() const;
  Trustchain::UserId const& userId() const;
  Crypto::Signature const& delegationSignature() const;
  Crypto::PublicSignatureKey const& publicSignatureKey() const;
  Crypto::PublicEncryptionKey const& publicEncryptionKey() const;
  bool isGhostDevice() const;
  nonstd::optional<UserKeyPair> userKeyPair() const;
  std::vector<Index> makeIndexes() const;

private:
  variant_type _v;
};

bool verifyDelegationSignature(
    DeviceCreation const&,
    Crypto::PublicSignatureKey const& publicSignatureKey);

constexpr std::size_t serialized_size(DeviceCreation1 const& dc)
{
  return detail::sizeOfCommonDeviceCreationFields(dc);
}

constexpr std::size_t serialized_size(DeviceCreation3 const& dc)
{
  return detail::sizeOfCommonDeviceCreationFields(dc) +
         serialized_size(dc.userKeyPair) + sizeof(dc.isGhostDevice);
}

std::size_t serialized_size(DeviceCreation const& dc);

void from_serialized(Serialization::SerializedSource& ss, DeviceCreation1&);
void from_serialized(Serialization::SerializedSource& ss, DeviceCreation3&);

std::uint8_t* to_serialized(std::uint8_t* it, DeviceCreation1 const& dc);
std::uint8_t* to_serialized(std::uint8_t* it, DeviceCreation3 const& dc);
std::uint8_t* to_serialized(std::uint8_t* it, DeviceCreation const& dc);

bool operator==(DeviceCreation1 const& l, DeviceCreation1 const& r);
bool operator!=(DeviceCreation1 const& l, DeviceCreation1 const& r);

bool operator==(DeviceCreation3 const& l, DeviceCreation3 const& r);
bool operator!=(DeviceCreation3 const& l, DeviceCreation3 const& r);

bool operator==(DeviceCreation const& l, DeviceCreation const& r);
bool operator!=(DeviceCreation const& l, DeviceCreation const& r);

void to_json(nlohmann::json& j, DeviceCreation1 const& dc);
void to_json(nlohmann::json& j, DeviceCreation3 const& dc);
void to_json(nlohmann::json& j, DeviceCreation const& dc);
}
