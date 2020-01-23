#pragma once

#include <Tanker/AttachResult.hpp>
#include <Tanker/Client.hpp>
#include <Tanker/DataStore/ADatabase.hpp>
#include <Tanker/Groups/Accessor.hpp>
#include <Tanker/Groups/Store.hpp>
#include <Tanker/Identity/PublicIdentity.hpp>
#include <Tanker/Identity/SecretProvisionalIdentity.hpp>
#include <Tanker/ProvisionalUsers/Accessor.hpp>
#include <Tanker/ProvisionalUsers/Manager.hpp>
#include <Tanker/ProvisionalUsers/ProvisionalUserKeysStore.hpp>
#include <Tanker/ResourceKeyAccessor.hpp>
#include <Tanker/ResourceKeyStore.hpp>
#include <Tanker/Streams/DecryptionStreamAdapter.hpp>
#include <Tanker/Streams/EncryptionStream.hpp>
#include <Tanker/Streams/InputSource.hpp>
#include <Tanker/Trustchain/Actions/DeviceCreation.hpp>
#include <Tanker/Trustchain/DeviceId.hpp>
#include <Tanker/Trustchain/GroupId.hpp>
#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/TrustchainStore.hpp>
#include <Tanker/Types/Email.hpp>
#include <Tanker/Types/Passphrase.hpp>
#include <Tanker/Types/SGroupId.hpp>
#include <Tanker/Types/SPublicIdentity.hpp>
#include <Tanker/Types/SResourceId.hpp>
#include <Tanker/Types/SSecretProvisionalIdentity.hpp>
#include <Tanker/Types/VerificationKey.hpp>
#include <Tanker/Unlock/Methods.hpp>
#include <Tanker/Unlock/Verification.hpp>
#include <Tanker/Users/ContactStore.hpp>
#include <Tanker/Users/Device.hpp>
#include <Tanker/Users/IRequester.hpp>
#include <Tanker/Users/LocalUser.hpp>
#include <Tanker/Users/Requester.hpp>
#include <Tanker/Users/UserAccessor.hpp>

#include <gsl-lite.hpp>
#include <tconcurrent/coroutine.hpp>
#include <tconcurrent/future.hpp>
#include <tconcurrent/promise.hpp>
#include <tconcurrent/task_auto_canceler.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Tanker
{
struct Entry;
struct UnverifiedEntry;

namespace Groups
{
class IRequester;
}

class Session
{
public:
  struct Config
  {
    DataStore::DatabasePtr db;
    Trustchain::TrustchainId trustchainId;
    Users::LocalUser::Ptr localUser;
    std::unique_ptr<Users::ContactStore> contactStore;
    std::unique_ptr<Client> client;
    std::unique_ptr<Users::Requester> userRequester;
  };
  using DeviceRevokedHandler = std::function<void()>;

  Session(Config&&);

  tc::cotask<void> encrypt(uint8_t* encryptedData,
                           gsl::span<uint8_t const> clearData,
                           std::vector<SPublicIdentity> const& suserIds = {},
                           std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<std::vector<uint8_t>> encrypt(
      gsl::span<uint8_t const> clearData,
      std::vector<SPublicIdentity> const& suserIds = {},
      std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<void> decrypt(uint8_t* decryptedData,
                           gsl::span<uint8_t const> encryptedData);

  tc::cotask<std::vector<uint8_t>> decrypt(
      gsl::span<uint8_t const> encryptedData);

  tc::cotask<void> share(std::vector<SResourceId> const& sresourceIds,
                         std::vector<SPublicIdentity> const& publicIdentities,
                         std::vector<SGroupId> const& groupIds);

  tc::cotask<SGroupId> createGroup(
      std::vector<SPublicIdentity> const& spublicIdentities);
  tc::cotask<void> updateGroupMembers(
      SGroupId const& groupIdString,
      std::vector<SPublicIdentity> const& spublicIdentitiesToAdd);

  tc::cotask<void> setVerificationMethod(Unlock::Verification const& method);
  tc::cotask<std::vector<Unlock::VerificationMethod>>
  fetchVerificationMethods();

  tc::cotask<AttachResult> attachProvisionalIdentity(
      SSecretProvisionalIdentity const& sidentity);
  tc::cotask<void> verifyProvisionalIdentity(
      Unlock::Verification const& verification);

  tc::cotask<void> revokeDevice(Trustchain::DeviceId const& deviceId);

  Trustchain::DeviceId const& deviceId() const;
  tc::cotask<std::vector<Users::Device>> getDeviceList() const;

  tc::cotask<Streams::EncryptionStream> makeEncryptionStream(
      Streams::InputSource,
      std::vector<SPublicIdentity> const& suserIds = {},
      std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<Streams::DecryptionStreamAdapter> makeDecryptionStream(
      Streams::InputSource);

  tc::cotask<void> nukeDatabase();

private:
  tc::cotask<void> updateLocalUser();
  Trustchain::UserId const& userId() const;
  Trustchain::TrustchainId const& trustchainId() const;
  Crypto::SymmetricKey const& userSecret() const;
  tc::cotask<Crypto::SymmetricKey> getResourceKey(
      Trustchain::ResourceId const&);

private:
  Trustchain::TrustchainId _trustchainId;
  DataStore::DatabasePtr _db;
  Users::LocalUser::Ptr _localUser;
  std::unique_ptr<Client> _client;
  std::unique_ptr<Users::Requester> _userRequester;
  std::unique_ptr<Groups::IRequester> _groupsRequester;
  TrustchainStore _trustchain;
  std::unique_ptr<Users::ContactStore> _contactStore;
  Groups::Store _groupStore;
  ResourceKeyStore _resourceKeyStore;
  ProvisionalUserKeysStore _provisionalUserKeysStore;

  mutable Users::UserAccessor _userAccessor;
  ProvisionalUsers::Accessor _provisionalUsersAccessor;
  ProvisionalUsers::Manager _provisionalUsersManager;
  Groups::Accessor _groupAccessor;
  ResourceKeyAccessor _resourceKeyAccessor;

  tc::promise<void> _ready;
};
}
