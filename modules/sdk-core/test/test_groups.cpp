#include <Tanker/Groups/Manager.hpp>

#include <Tanker/Crypto/Format/Format.hpp>
#include <Tanker/Serialization/Serialization.hpp>
#include <Tanker/Trustchain/DeviceId.hpp>
#include <Tanker/Trustchain/ServerEntry.hpp>
#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/UserNotFound.hpp>

#include <Helpers/Await.hpp>

#include "TestVerifier.hpp"
#include "TrustchainBuilder.hpp"
#include "UserAccessorMock.hpp"

#include <doctest.h>

#include <mockaron/mockaron.hpp>

#include <trompeloeil.hpp>

#include <Helpers/Buffers.hpp>

using namespace Tanker;
using namespace Tanker::Trustchain::Actions;

TEST_CASE("Can't create an empty group")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");

  auto const user = *builder.getUser("user");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  auto groupEncryptionKey = Crypto::makeEncryptionKeyPair();
  auto groupSignatureKey = Crypto::makeSignatureKeyPair();

  CHECK_THROWS_AS(
      AWAIT(Groups::Manager::generateCreateGroupBlock(
          {}, {}, userBlockGenerator, groupSignatureKey, groupEncryptionKey)),
      Error::InvalidGroupSize);
}

TEST_CASE("Can create a group with two users")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");
  builder.makeUser3("user2");

  auto const user = *builder.getUser("user");
  auto const user2 = *builder.getUser("user2");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  auto groupEncryptionKey = Crypto::makeEncryptionKeyPair();
  auto groupSignatureKey = Crypto::makeSignatureKeyPair();

  auto const preserializedBlock =
      AWAIT(Groups::Manager::generateCreateGroupBlock(
          {user.asTankerUser(), user2.asTankerUser()},
          {},
          userBlockGenerator,
          groupSignatureKey,
          groupEncryptionKey));

  auto block = Serialization::deserialize<Block>(preserializedBlock);
  auto entry = blockToServerEntry(block);
  auto group =
      entry.action().get<UserGroupCreation>().get<UserGroupCreation2>();

  auto const selfSignature =
      Crypto::sign(group.signatureData(), groupSignatureKey.privateKey);

  CHECK(group.publicSignatureKey() == groupSignatureKey.publicKey);
  CHECK(group.publicEncryptionKey() == groupEncryptionKey.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateSignatureKey>(
            group.sealedPrivateSignatureKey(), groupEncryptionKey) ==
        groupSignatureKey.privateKey);
  REQUIRE(group.userGroupMembers().size() == 2);
  REQUIRE(group.userGroupProvisionalMembers().size() == 0);
  auto const groupEncryptedKey =
      std::find_if(group.userGroupMembers().begin(),
                   group.userGroupMembers().end(),
                   [&](auto const& groupEncryptedKey) {
                     return groupEncryptedKey.userId() == user.userId;
                   });
  REQUIRE(groupEncryptedKey != group.userGroupMembers().end());
  CHECK(groupEncryptedKey->userPublicKey() ==
        user.userKeys.back().keyPair.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateEncryptionKey>(
            groupEncryptedKey->encryptedPrivateEncryptionKey(),
            user.userKeys.back().keyPair) == groupEncryptionKey.privateKey);
  CHECK(selfSignature == group.selfSignature());
}

TEST_CASE("Can create a group with two provisional users")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");
  auto const user = *builder.getUser("user");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  auto const provisionalUser = builder.makeProvisionalUser("bob@tanker");
  auto const provisionalUser2 = builder.makeProvisionalUser("charlie@tanker");

  auto groupEncryptionKey = Crypto::makeEncryptionKeyPair();
  auto groupSignatureKey = Crypto::makeSignatureKeyPair();

  auto const preserializedBlock =
      AWAIT(Groups::Manager::generateCreateGroupBlock(
          {},
          {builder.toPublicProvisionalUser(provisionalUser),
           builder.toPublicProvisionalUser(provisionalUser2)},
          userBlockGenerator,
          groupSignatureKey,
          groupEncryptionKey));

  auto block = Serialization::deserialize<Block>(preserializedBlock);
  auto entry = blockToServerEntry(block);
  auto group =
      entry.action().get<UserGroupCreation>().get<UserGroupCreation2>();

  auto const selfSignature =
      Crypto::sign(group.signatureData(), groupSignatureKey.privateKey);

  CHECK(group.publicSignatureKey() == groupSignatureKey.publicKey);
  CHECK(group.publicEncryptionKey() == groupEncryptionKey.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateSignatureKey>(
            group.sealedPrivateSignatureKey(), groupEncryptionKey) ==
        groupSignatureKey.privateKey);
  REQUIRE(group.userGroupMembers().size() == 0);
  REQUIRE(group.userGroupProvisionalMembers().size() == 2);
  auto const groupEncryptedKey =
      std::find_if(group.userGroupProvisionalMembers().begin(),
                   group.userGroupProvisionalMembers().end(),
                   [&](auto const& groupEncryptedKey) {
                     return groupEncryptedKey.appPublicSignatureKey() ==
                            provisionalUser.appSignatureKeyPair.publicKey;
                   });
  REQUIRE(groupEncryptedKey != group.userGroupProvisionalMembers().end());
  CHECK(groupEncryptedKey->tankerPublicSignatureKey() ==
        provisionalUser.tankerSignatureKeyPair.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateEncryptionKey>(
            Crypto::sealDecrypt(
                groupEncryptedKey->encryptedPrivateEncryptionKey(),
                provisionalUser.tankerEncryptionKeyPair),
            provisionalUser.appEncryptionKeyPair) ==
        groupEncryptionKey.privateKey);
  CHECK(selfSignature == group.selfSignature());
}

TEST_CASE("throws when getting keys of an unknown member")
{
  auto const unknownUid = make<Trustchain::UserId>("unknown");

  mockaron::mock<UserAccessor, UserAccessorMock> userAccessor;

  REQUIRE_CALL(
      userAccessor.get_mock_impl(),
      pull(trompeloeil::eq(gsl::span<Trustchain::UserId const>{unknownUid})))
      .LR_RETURN((UserAccessor::PullResult{{}, {unknownUid}}));

  REQUIRE_THROWS_AS(
      AWAIT(Groups::Manager::getMemberKeys(userAccessor.get(), {unknownUid})),
      Error::UserNotFoundInternal);
}

TEST_CASE("Fails to add 0 users to a group")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");

  auto const user = *builder.getUser("user");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  Group const group{};

  CHECK_THROWS_AS(AWAIT(Groups::Manager::generateAddUserToGroupBlock(
                      {}, {}, userBlockGenerator, group)),
                  Error::InvalidGroupSize);
}

TEST_CASE("Can add users to a group")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");
  builder.makeUser3("user2");

  auto const user = *builder.getUser("user");
  auto const user2 = *builder.getUser("user2");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  auto const groupResult = builder.makeGroup(userDevice, {user, user2});
  auto group = groupResult.group.tankerGroup;

  auto const preserializedBlock =
      AWAIT(Groups::Manager::generateAddUserToGroupBlock(
          {user.asTankerUser(), user2.asTankerUser()},
          {},
          userBlockGenerator,
          group));

  auto block = Serialization::deserialize<Block>(preserializedBlock);
  auto entry = blockToServerEntry(block);
  auto groupAdd =
      entry.action().get<UserGroupAddition>().get<UserGroupAddition::v2>();

  auto const selfSignature =
      Crypto::sign(groupAdd.signatureData(), group.signatureKeyPair.privateKey);

  CHECK(groupAdd.groupId() ==
        Trustchain::GroupId{group.signatureKeyPair.publicKey});
  CHECK(groupAdd.previousGroupBlockHash() == group.lastBlockHash);
  REQUIRE(groupAdd.members().size() == 2);
  REQUIRE(groupAdd.provisionalMembers().size() == 0);

  auto const groupEncryptedKey =
      std::find_if(groupAdd.members().begin(),
                   groupAdd.members().end(),
                   [&](auto const& groupEncryptedKey) {
                     return groupEncryptedKey.userId() == user.userId;
                   });
  REQUIRE(groupEncryptedKey != groupAdd.members().end());
  CHECK(groupEncryptedKey->userPublicKey() ==
        user.userKeys.back().keyPair.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateEncryptionKey>(
            groupEncryptedKey->encryptedPrivateEncryptionKey(),
            user.userKeys.back().keyPair) ==
        group.encryptionKeyPair.privateKey);
  CHECK(selfSignature == groupAdd.selfSignature());
}

TEST_CASE("Can add provisional users to a group")
{
  TrustchainBuilder builder;
  builder.makeUser3("user");
  auto const user = *builder.getUser("user");
  auto const userDevice = user.devices.front();
  auto const userBlockGenerator = builder.makeBlockGenerator(userDevice);

  auto const groupResult = builder.makeGroup(userDevice, {user});
  auto group = groupResult.group.tankerGroup;

  auto const provisionalUser = builder.makeProvisionalUser("bob@tanker");
  auto const provisionalUser2 = builder.makeProvisionalUser("charlie@tanker");

  auto const preserializedBlock =
      AWAIT(Groups::Manager::generateAddUserToGroupBlock(
          {},
          {builder.toPublicProvisionalUser(provisionalUser),
           builder.toPublicProvisionalUser(provisionalUser2)},
          userBlockGenerator,
          group));

  auto block = Serialization::deserialize<Block>(preserializedBlock);
  auto entry = blockToServerEntry(block);
  auto groupAdd =
      entry.action().get<UserGroupAddition>().get<UserGroupAddition::v2>();

  auto const selfSignature =
      Crypto::sign(groupAdd.signatureData(), group.signatureKeyPair.privateKey);

  CHECK(groupAdd.groupId() ==
        Trustchain::GroupId{group.signatureKeyPair.publicKey});
  CHECK(groupAdd.previousGroupBlockHash() == group.lastBlockHash);
  REQUIRE(groupAdd.members().size() == 0);
  REQUIRE(groupAdd.provisionalMembers().size() == 2);

  auto const groupEncryptedKey =
      std::find_if(groupAdd.provisionalMembers().begin(),
                   groupAdd.provisionalMembers().end(),
                   [&](auto const& groupEncryptedKey) {
                     return groupEncryptedKey.appPublicSignatureKey() ==
                            provisionalUser.appSignatureKeyPair.publicKey;
                   });
  REQUIRE(groupEncryptedKey != groupAdd.provisionalMembers().end());
  CHECK(groupEncryptedKey->tankerPublicSignatureKey() ==
        provisionalUser.tankerSignatureKeyPair.publicKey);
  CHECK(Crypto::sealDecrypt<Crypto::PrivateEncryptionKey>(
            Crypto::sealDecrypt(
                groupEncryptedKey->encryptedPrivateEncryptionKey(),
                provisionalUser.tankerEncryptionKeyPair),
            provisionalUser.appEncryptionKeyPair) ==
        group.encryptionKeyPair.privateKey);
  CHECK(selfSignature == groupAdd.selfSignature());
}
