#pragma once

#include <Tanker/Actions/DeviceRevocation.hpp>
#include <Tanker/Actions/UserGroupAddition.hpp>
#include <Tanker/Actions/UserGroupCreation.hpp>
#include <Tanker/Block.hpp>
#include <Tanker/Crypto/Types.hpp>
#include <Tanker/Nature.hpp>
#include <Tanker/Types/DeviceId.hpp>
#include <Tanker/Types/TrustchainId.hpp>

#include <cstdint>
#include <vector>

namespace Tanker
{
namespace Identity
{
struct Delegation;
}

class BlockGenerator
{
public:
  BlockGenerator(TrustchainId const& trustchainId,
                 Crypto::PrivateSignatureKey const& privateSignatureKey,
                 DeviceId const& deviceId);

  TrustchainId const& trustchainId() const noexcept;
  Crypto::PrivateSignatureKey const& signatureKey() const noexcept;

  void setDeviceId(DeviceId const& deviceId);
  DeviceId const& deviceId() const;

  std::vector<uint8_t> addUser(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey) const;
  std::vector<uint8_t> addUser1(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey) const;
  std::vector<uint8_t> addUser3(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey) const;

  std::vector<uint8_t> addDevice(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey) const;
  std::vector<uint8_t> addDevice1(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey) const;
  std::vector<uint8_t> addDevice3(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey) const;

  std::vector<uint8_t> addGhostDevice(
      Identity::Delegation const& delegation,
      Crypto::PublicSignatureKey const& signatureKey,
      Crypto::PublicEncryptionKey const& encryptionKey,
      Crypto::EncryptionKeyPair const& userEncryptionKey) const;

  std::vector<uint8_t> revokeDevice2(
      DeviceId const& deviceId,
      Crypto::PublicEncryptionKey const& publicEncryptionKey,
      Crypto::PublicEncryptionKey const& previousPublicEncryptionKey,
      Crypto::SealedPrivateEncryptionKey const& encryptedKeyForPreviousUserKey,
      std::vector<EncryptedPrivateUserKey> const& userKeys) const;

  std::vector<uint8_t> keyPublish(Crypto::EncryptedSymmetricKey const& symKey,
                                  Crypto::Mac const& mac,
                                  DeviceId const& recipient) const;

  std::vector<uint8_t> keyPublishToUser(
      Crypto::SealedSymmetricKey const& symKey,
      Crypto::Mac const& mac,
      Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey) const;

  std::vector<uint8_t> keyPublishToGroup(
      Crypto::SealedSymmetricKey const& symKey,
      Crypto::Mac const& mac,
      Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey) const;

  std::vector<uint8_t> userGroupCreation(
      Crypto::SignatureKeyPair const& signatureKeyPair,
      Crypto::PublicEncryptionKey const& publicEncryptionKey,
      UserGroupCreation::GroupEncryptedKeys const&
          encryptedGroupPrivateEncryptionKeysForUsers) const;

  std::vector<uint8_t> userGroupAddition(
      Crypto::SignatureKeyPair const& signatureKeyPair,
      Crypto::Hash const& previousGroupBlock,
      UserGroupCreation::GroupEncryptedKeys const&
          encryptedGroupPrivateEncryptionKeysForUsers) const;

private:
  TrustchainId _trustchainId;
  Crypto::PrivateSignatureKey _privateSignatureKey;
  DeviceId _deviceId;

  template <typename T, typename U>
  Block makeBlock(Nature nature,
                  T const& action,
                  Crypto::BasicHash<U> const& parentHash,
                  Crypto::PrivateSignatureKey const& privateSignatureKey) const;
  template <typename T, typename U>
  Block makeBlock(T const& action,
                  Crypto::BasicHash<U> const& parentHash,
                  Crypto::PrivateSignatureKey const& privateSignatureKey) const;
};
}
