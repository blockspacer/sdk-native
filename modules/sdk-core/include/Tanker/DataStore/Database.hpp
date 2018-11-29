#pragma once

#include <Tanker/Crypto/Types.hpp>
#include <Tanker/DataStore/Connection.hpp>
#include <Tanker/Device.hpp>
#include <Tanker/DeviceKeys.hpp>
#include <Tanker/Entry.hpp>
#include <Tanker/Groups/Group.hpp>
#include <Tanker/Types/DeviceId.hpp>
#include <Tanker/Types/UserId.hpp>

#include <sqlpp11/sqlite3/connection.h>

#include <tconcurrent/coroutine.hpp>

#include <optional.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Tanker
{
namespace DataStore
{
class RecordNotFound : public std::exception
{
public:
  RecordNotFound(std::string msg) : _msg(std::move(msg))
  {
  }

  char const* what() const noexcept override
  {
    return _msg.c_str();
  }

private:
  std::string _msg;
};

class Database
{
public:
  explicit Database(std::string const& dbPath,
                    nonstd::optional<Crypto::SymmetricKey> const& userSecret,
                    bool exclusive);

  tc::cotask<void> inTransaction(std::function<tc::cotask<void>()> const& f);

  tc::cotask<void> putUserPrivateKey(
      Crypto::PublicEncryptionKey const& publicKey,
      Crypto::PrivateEncryptionKey const& privateKey);
  tc::cotask<Crypto::EncryptionKeyPair> getUserKeyPair(
      Crypto::PublicEncryptionKey const& publicKey);
  tc::cotask<nonstd::optional<Crypto::EncryptionKeyPair>>
  getUserOptLastKeyPair();

  tc::cotask<uint64_t> getTrustchainLastIndex();
  tc::cotask<void> addTrustchainEntry(Entry const& Entry);
  tc::cotask<nonstd::optional<Entry>> findTrustchainEntry(
      Crypto::Hash const& hash) const;
  tc::cotask<nonstd::optional<Entry>> findTrustchainKeyPublish(
      Crypto::Mac const& resourceId);
  tc::cotask<std::vector<Entry>> getTrustchainDevicesOf(UserId const& userId);
  tc::cotask<Entry> getTrustchainDevice(DeviceId const& deviceId);

  tc::cotask<void> putContact(
      UserId const& userId,
      nonstd::optional<Crypto::PublicEncryptionKey> const& publicKey);

  tc::cotask<nonstd::optional<Crypto::PublicEncryptionKey>> getContactUserKey(
      UserId const& userId);

  tc::cotask<void> putResourceKey(Crypto::Mac const& mac,
                                  Crypto::SymmetricKey const& key);
  tc::cotask<nonstd::optional<Crypto::SymmetricKey>> findResourceKey(
      Crypto::Mac const& mac);

  tc::cotask<nonstd::optional<DeviceKeys>> getDeviceKeys();
  tc::cotask<void> setDeviceKeys(DeviceKeys const& deviceKeys);
  tc::cotask<void> setDeviceId(DeviceId const& deviceId);

  tc::cotask<void> putDevice(UserId const& userId, Device const& device);
  tc::cotask<nonstd::optional<Device>> getOptDevice(DeviceId const& id) const;
  tc::cotask<std::vector<Device>> getDevicesOf(UserId const& id) const;

  tc::cotask<void> putFullGroup(Group const& group);
  tc::cotask<void> putExternalGroup(ExternalGroup const& group);
  // Does nothing if the group does not exist
  tc::cotask<void> updateLastGroupBlock(GroupId const& groupId,
                                        Crypto::Hash const& lastBlockHash,
                                        uint64_t lastBlockIndex);
  tc::cotask<nonstd::optional<Group>> findFullGroupByGroupId(
      GroupId const& groupId) const;
  tc::cotask<nonstd::optional<ExternalGroup>> findExternalGroupByGroupId(
      GroupId const& groupId) const;
  tc::cotask<nonstd::optional<Group>> findFullGroupByGroupPublicEncryptionKey(
      Crypto::PublicEncryptionKey const& publicEncryptionKey) const;
  tc::cotask<nonstd::optional<ExternalGroup>>
  findExternalGroupByGroupPublicEncryptionKey(
      Crypto::PublicEncryptionKey const& publicEncryptionKey) const;

private:
  ConnPtr _db;

  std::vector<sqlpp::transaction_t<sqlpp::sqlite3::connection>> _transactions;

  bool isMigrationNeeded();
  void flushAllCaches();
  tc::cotask<void> indexKeyPublish(Crypto::Hash const& hash,
                                   Crypto::Mac const& resourceId);

  tc::cotask<void> startTransaction();
  tc::cotask<void> commitTransaction();
  tc::cotask<void> rollbackTransaction();
};

using DatabasePtr = std::unique_ptr<Database>;

inline tc::cotask<DatabasePtr> createDatabase(
    std::string const& dbPath,
    nonstd::optional<Crypto::SymmetricKey> const& userSecret = {},
    bool exclusive = true)
{
  TC_RETURN(std::make_unique<Database>(dbPath, userSecret, exclusive));
}
}
}
