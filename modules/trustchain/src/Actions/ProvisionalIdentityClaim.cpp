#include <Tanker/Trustchain/Actions/ProvisionalIdentityClaim.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Serialization/Serialization.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace Tanker
{
namespace Trustchain
{
namespace Actions
{
ProvisionalIdentityClaim::ProvisionalIdentityClaim(
    UserId const& userId,
    Crypto::PublicSignatureKey const& appSignaturePublicKey,
    Crypto::Signature const& authorSignatureByAppKey,
    Crypto::PublicSignatureKey const& tankerSignaturePublicKey,
    Crypto::Signature const& authorSignatureByTankerKey,
    Crypto::PublicEncryptionKey const& userPublicEncryptionKey,
    SealedPrivateEncryptionKeys const& sealedPrivateEncryptionKeys)
  : _userId(userId),
    _appSignaturePublicKey(appSignaturePublicKey),
    _tankerSignaturePublicKey(tankerSignaturePublicKey),
    _authorSignatureByAppKey(authorSignatureByAppKey),
    _authorSignatureByTankerKey(authorSignatureByTankerKey),
    _userPublicEncryptionKey(userPublicEncryptionKey),
    _sealedPrivateEncryptionKeys(sealedPrivateEncryptionKeys)
{
}

ProvisionalIdentityClaim::ProvisionalIdentityClaim(
    UserId const& userId,
    Crypto::PublicSignatureKey const& appSignaturePublicKey,
    Crypto::PublicSignatureKey const& tankerSignaturePublicKey,
    Crypto::PublicEncryptionKey const& userPublicEncryptionKey,
    SealedPrivateEncryptionKeys const& sealedPrivateEncryptionKeys)
  : _userId(userId),
    _appSignaturePublicKey(appSignaturePublicKey),
    _tankerSignaturePublicKey(tankerSignaturePublicKey),
    _userPublicEncryptionKey(userPublicEncryptionKey),
    _sealedPrivateEncryptionKeys(sealedPrivateEncryptionKeys)
{
}

UserId const& ProvisionalIdentityClaim::userId() const
{
  return _userId;
}

Crypto::PublicSignatureKey const&
ProvisionalIdentityClaim::appSignaturePublicKey() const
{
  return _appSignaturePublicKey;
}

Crypto::PublicSignatureKey const&
ProvisionalIdentityClaim::tankerSignaturePublicKey() const
{
  return _tankerSignaturePublicKey;
}

Crypto::Signature const& ProvisionalIdentityClaim::authorSignatureByAppKey()
    const
{
  return _authorSignatureByAppKey;
}

Crypto::Signature const& ProvisionalIdentityClaim::authorSignatureByTankerKey()
    const
{
  return _authorSignatureByTankerKey;
}

Crypto::PublicEncryptionKey const&
ProvisionalIdentityClaim::userPublicEncryptionKey() const
{
  return _userPublicEncryptionKey;
}

auto ProvisionalIdentityClaim::sealedPrivateEncryptionKeys() const
    -> SealedPrivateEncryptionKeys const&
{
  return _sealedPrivateEncryptionKeys;
}

std::vector<std::uint8_t> ProvisionalIdentityClaim::signatureData(
    DeviceId const& authorId) const
{
  std::vector<std::uint8_t> signatureData(
      DeviceId::arraySize + (Crypto::PublicSignatureKey::arraySize * 2));

  auto it = std::copy(authorId.begin(), authorId.end(), signatureData.begin());
  it = std::copy(
      _appSignaturePublicKey.begin(), _appSignaturePublicKey.end(), it);
  std::copy(
      _tankerSignaturePublicKey.begin(), _tankerSignaturePublicKey.end(), it);
  return signatureData;
}

Crypto::Signature const& ProvisionalIdentityClaim::signWithAppKey(
    Crypto::PrivateSignatureKey const& privateKey, DeviceId const& authorId)
{
  auto const toSign = signatureData(authorId);

  return _authorSignatureByAppKey = Crypto::sign(toSign, privateKey);
}

Crypto::Signature const& ProvisionalIdentityClaim::signWithTankerKey(
    Crypto::PrivateSignatureKey const& privateKey, DeviceId const& authorId)
{
  auto const toSign = signatureData(authorId);

  return _authorSignatureByTankerKey = Crypto::sign(toSign, privateKey);
}

bool operator==(ProvisionalIdentityClaim const& lhs,
                ProvisionalIdentityClaim const& rhs)
{
  return std::tie(lhs.userId(),
                  lhs.appSignaturePublicKey(),
                  lhs.tankerSignaturePublicKey(),
                  lhs.authorSignatureByAppKey(),
                  lhs.authorSignatureByTankerKey(),
                  lhs.userPublicEncryptionKey(),
                  lhs.sealedPrivateEncryptionKeys()) ==
         std::tie(rhs.userId(),
                  rhs.appSignaturePublicKey(),
                  rhs.tankerSignaturePublicKey(),
                  rhs.authorSignatureByAppKey(),
                  rhs.authorSignatureByTankerKey(),
                  rhs.userPublicEncryptionKey(),
                  rhs.sealedPrivateEncryptionKeys());
}

bool operator!=(ProvisionalIdentityClaim const& lhs,
                ProvisionalIdentityClaim const& rhs)
{
  return !(lhs == rhs);
}

void from_serialized(Serialization::SerializedSource& ss,
                     ProvisionalIdentityClaim& pic)
{
  Serialization::deserialize_to(ss, pic._userId);
  Serialization::deserialize_to(ss, pic._appSignaturePublicKey);
  Serialization::deserialize_to(ss, pic._tankerSignaturePublicKey);
  Serialization::deserialize_to(ss, pic._authorSignatureByAppKey);
  Serialization::deserialize_to(ss, pic._authorSignatureByTankerKey);
  Serialization::deserialize_to(ss, pic._userPublicEncryptionKey);
  Serialization::deserialize_to(ss, pic._sealedPrivateEncryptionKeys);
}

std::uint8_t* to_serialized(std::uint8_t* it,
                            ProvisionalIdentityClaim const& pic)
{
  it = Serialization::serialize(it, pic.userId());
  it = Serialization::serialize(it, pic.appSignaturePublicKey());
  it = Serialization::serialize(it, pic.tankerSignaturePublicKey());
  it = Serialization::serialize(it, pic.authorSignatureByAppKey());
  it = Serialization::serialize(it, pic.authorSignatureByTankerKey());
  it = Serialization::serialize(it, pic.userPublicEncryptionKey());
  it = Serialization::serialize(it, pic.sealedPrivateEncryptionKeys());
  return it;
}

void to_json(nlohmann::json& j, ProvisionalIdentityClaim const& pic)
{
  j["userId"] = pic.userId();
  j["appSignaturePublicKey"] = pic.appSignaturePublicKey();
  j["tankerSignaturePublicKey"] = pic.tankerSignaturePublicKey();
  j["authorSignatureByAppKey"] = pic.authorSignatureByAppKey();
  j["authorSignatureByTankerKey"] = pic.authorSignatureByTankerKey();
  j["userPublicEncryptionKey"] = pic.userPublicEncryptionKey();
  j["sealedPrivateEncryptionKeys"] = pic.sealedPrivateEncryptionKeys();
}
}
}
}
