#pragma once

#include <Tanker/Entry.hpp>
#include <Tanker/Types/TrustchainId.hpp>

#include <tconcurrent/coroutine.hpp>

namespace Tanker
{
struct UnverifiedEntry;
class ContactStore;
class GroupStore;
struct ExternalGroup;
struct User;
struct Device;

namespace DataStore
{
class Database;
}

class TrustchainVerifier
{
public:
  TrustchainVerifier(TrustchainId const&,
                     DataStore::Database*,
                     ContactStore*,
                     GroupStore*);

  TrustchainVerifier(TrustchainVerifier const&) = delete;
  TrustchainVerifier(TrustchainVerifier&&) = delete;
  TrustchainVerifier& operator=(TrustchainVerifier const&) = delete;
  TrustchainVerifier& operator=(TrustchainVerifier&&) = delete;

  tc::cotask<Entry> verify(UnverifiedEntry const&) const;

private:
  tc::cotask<Entry> handleDeviceCreation(UnverifiedEntry const& dc) const;
  tc::cotask<Entry> handleKeyPublish(UnverifiedEntry const& dc) const;
  tc::cotask<Entry> handleKeyPublishToUserGroups(
      UnverifiedEntry const& kp) const;
  tc::cotask<Entry> handleDeviceRevocation(UnverifiedEntry const& dr) const;
  tc::cotask<Entry> handleUserGroupAddition(UnverifiedEntry const& ga) const;
  tc::cotask<Entry> handleUserGroupCreation(UnverifiedEntry const& gc) const;
  tc::cotask<Entry> getAuthor(Crypto::Hash const& authorHash) const;
  tc::cotask<User> getUser(UserId const& userId) const;
  Device getDevice(User const& user, Crypto::Hash const& deviceHash) const;
  tc::cotask<ExternalGroup> getGroupByEncryptionKey(
      Crypto::PublicEncryptionKey const& recipientPublicEncryprionKey) const;
  tc::cotask<ExternalGroup> getGroupById(GroupId const& groupId) const;

  TrustchainId _trustchainId;
  DataStore::Database* _db;
  ContactStore* _contacts;
  GroupStore* _groups;
};
}
