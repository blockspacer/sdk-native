#include <Tanker/Groups/Updater.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/DataStore/ADatabase.hpp>

#include <Helpers/Await.hpp>
#include <Helpers/Buffers.hpp>

#include "FakeProvisionalUsersAccessor.hpp"
#include "TestVerifier.hpp"
#include "TrustchainBuilder.hpp"

#include <doctest.h>

using namespace Tanker;

namespace
{
void testUserGroupCreationCommon(
    std::function<TrustchainBuilder::ResultGroup(
        TrustchainBuilder&,
        TrustchainBuilder::Device const&,
        std::vector<TrustchainBuilder::User> const&)> const& makeGroup)
{
  auto const aliceDb = AWAIT(DataStore::createDatabase(":memory:"));

  TrustchainBuilder builder;
  auto const alice = builder.makeUser3("alice");
  auto const aliceLocalUser = builder.makeLocalUser(alice.user, aliceDb.get());
  auto const aliceProvisionalUserKeysStore =
      builder.makeProvisionalUserKeysStoreWith({}, aliceDb.get());
  auto const aliceProvisionalUsersAccessor =
      std::make_unique<FakeProvisionalUsersAccessor>(
          *aliceProvisionalUserKeysStore);

  SUBCASE("handles creation of a group I am part of")
  {
    auto const group = makeGroup(builder, alice.user.devices[0], {alice.user});
    auto const resultGroup = AWAIT(
        GroupUpdater::applyUserGroupCreation(*aliceLocalUser,
                                             *aliceProvisionalUsersAccessor,
                                             toVerifiedEntry(group.entry)));
    CHECK_EQ(resultGroup, Group{group.group.tankerGroup});
  }

  SUBCASE("handles creation of a group I am *not* part of")
  {
    auto const bob = builder.makeUser3("bob");

    auto const group = makeGroup(builder, bob.user.devices[0], {bob.user});
    auto const resultGroup = AWAIT(
        GroupUpdater::applyUserGroupCreation(*aliceLocalUser,
                                             *aliceProvisionalUsersAccessor,
                                             toVerifiedEntry(group.entry)));

    CHECK_EQ(resultGroup, Group{group.group.asExternalGroup()});
  }
}

void testUserGroupAdditionCommon(
    std::function<TrustchainBuilder::ResultGroup(
        TrustchainBuilder&,
        TrustchainBuilder::Device const&,
        TrustchainBuilder::InternalGroup const&,
        std::vector<TrustchainBuilder::User> const&)> const& addUserToGroup)
{
  auto const aliceDb = AWAIT(DataStore::createDatabase(":memory:"));

  TrustchainBuilder builder;
  auto const alice = builder.makeUser3("alice");
  auto const bob = builder.makeUser3("bob");
  auto const aliceLocalUser = builder.makeLocalUser(alice.user, aliceDb.get());
  auto const aliceProvisionalUserKeysStore =
      builder.makeProvisionalUserKeysStoreWith({}, aliceDb.get());
  auto const aliceProvisionalUsersAccessor =
      std::make_unique<FakeProvisionalUsersAccessor>(
          *aliceProvisionalUserKeysStore);

  SUBCASE("Alice sees Bob being added to her group")
  {
    auto const aliceGroup =
        builder.makeGroup2(alice.user.devices[0], {alice.user}, {});
    auto const updatedGroup = addUserToGroup(
        builder, alice.user.devices[0], aliceGroup.group, {bob.user});
    auto const resultGroup = AWAIT(GroupUpdater::applyUserGroupAddition(
        *aliceLocalUser,
        *aliceProvisionalUsersAccessor,
        aliceGroup.group.tankerGroup,
        toVerifiedEntry(updatedGroup.entry)));
    CHECK_EQ(resultGroup, Group{updatedGroup.group.tankerGroup});
  }

  SUBCASE("Alice sees herself being added to Bob's group")
  {
    auto const bobGroup =
        builder.makeGroup2(bob.user.devices[0], {bob.user}, {});
    auto const updatedGroup = addUserToGroup(
        builder, bob.user.devices[0], bobGroup.group, {alice.user});
    auto const resultGroup = AWAIT(GroupUpdater::applyUserGroupAddition(
        *aliceLocalUser,
        *aliceProvisionalUsersAccessor,
        bobGroup.group.asExternalGroup(),
        toVerifiedEntry(updatedGroup.entry)));
    CHECK_EQ(resultGroup, Group{updatedGroup.group.tankerGroup});
  }

  SUBCASE("Alice sees Charly being added to Bob's group")
  {
    auto const bobGroup =
        builder.makeGroup2(bob.user.devices[0], {bob.user}, {});
    auto const charly = builder.makeUser3("charly");
    auto const updatedGroup = addUserToGroup(
        builder, bob.user.devices[0], bobGroup.group, {charly.user});
    auto const resultGroup = AWAIT(GroupUpdater::applyUserGroupAddition(
        *aliceLocalUser,
        *aliceProvisionalUsersAccessor,
        bobGroup.group.asExternalGroup(),
        toVerifiedEntry(updatedGroup.entry)));

    CHECK_EQ(resultGroup, Group{updatedGroup.group.asExternalGroup()});
  }
}
}

TEST_CASE("GroupUpdater UserGroupCreation::v1")
{
  testUserGroupCreationCommon(
      [](TrustchainBuilder& builder,
         TrustchainBuilder::Device const& authorDevice,
         std::vector<TrustchainBuilder::User> const& members) {
        return builder.makeGroup1(authorDevice, members);
      });
}

TEST_CASE("GroupUpdater UserGroupCreation::v2")
{
  SUBCASE("Common checks")
  {
    testUserGroupCreationCommon(
        [](TrustchainBuilder& builder,
           TrustchainBuilder::Device const& authorDevice,
           std::vector<TrustchainBuilder::User> const& members) {
          return builder.makeGroup2(authorDevice, members, {});
        });
  }

  SUBCASE("Specific checks")
  {
    auto const aliceDb = AWAIT(DataStore::createDatabase(":memory:"));

    TrustchainBuilder builder;
    auto const alice = builder.makeUser3("alice");
    auto const aliceLocalUser =
        builder.makeLocalUser(alice.user, aliceDb.get());
    auto const aliceProvisionalUser =
        builder.makeProvisionalUser("alice@tanker");
    auto const aliceProvisionalUserKeysStore =
        builder.makeProvisionalUserKeysStoreWith({aliceProvisionalUser},
                                                 aliceDb.get());
    auto const aliceProvisionalUsersAccessor =
        std::make_unique<FakeProvisionalUsersAccessor>(
            *aliceProvisionalUserKeysStore);

    SUBCASE(
        "handles creation of a group I am part of through a provisional "
        "identity")
    {
      auto const bob = builder.makeUser3("bob");

      auto const group =
          builder.makeGroup2(bob.user.devices[0],
                             {},
                             {aliceProvisionalUser.publicProvisionalUser});
      auto const resultGroup = AWAIT(
          GroupUpdater::applyUserGroupCreation(*aliceLocalUser,
                                               *aliceProvisionalUsersAccessor,
                                               toVerifiedEntry(group.entry)));

      CHECK_EQ(resultGroup, Group{group.group.tankerGroup});
    }
  }
}

TEST_CASE("GroupUpdater UserGroupAddition1")
{
  testUserGroupAdditionCommon(
      [](TrustchainBuilder& builder,
         TrustchainBuilder::Device const& authorDevice,
         TrustchainBuilder::InternalGroup const& group,
         std::vector<TrustchainBuilder::User> const& members) {
        return builder.addUserToGroup(authorDevice, group, members);
      });
}

TEST_CASE("GroupUpdater UserGroupAddition2")
{
  SUBCASE("Common checks")
  {
    testUserGroupAdditionCommon(
        [](TrustchainBuilder& builder,
           TrustchainBuilder::Device const& authorDevice,
           TrustchainBuilder::InternalGroup const& group,
           std::vector<TrustchainBuilder::User> const& members) {
          return builder.addUserToGroup2(authorDevice, group, members, {});
        });
  }

  SUBCASE("Specific checks")
  {
    auto const aliceDb = AWAIT(DataStore::createDatabase(":memory:"));

    TrustchainBuilder builder;
    auto const alice = builder.makeUser3("alice");
    auto const bob = builder.makeUser3("bob");
    auto const aliceLocalUser =
        builder.makeLocalUser(alice.user, aliceDb.get());
    auto const aliceProvisionalUser =
        builder.makeProvisionalUser("alice@tanker");
    auto const aliceProvisionalUserKeysStore =
        builder.makeProvisionalUserKeysStoreWith({aliceProvisionalUser},
                                                 aliceDb.get());
    auto const aliceProvisionalUsersAccessor =
        std::make_unique<FakeProvisionalUsersAccessor>(
            *aliceProvisionalUserKeysStore);

    SUBCASE(
        "Alice sees herself being added to Bob's group as a provisional user")
    {
      auto const bobGroup =
          builder.makeGroup2(bob.user.devices[0], {bob.user}, {});
      auto const updatedGroup =
          builder.addUserToGroup2(bob.user.devices[0],
                                  bobGroup.group,
                                  {},
                                  {aliceProvisionalUser.publicProvisionalUser});
      auto const resultGroup = AWAIT(GroupUpdater::applyUserGroupAddition(
          *aliceLocalUser,
          *aliceProvisionalUsersAccessor,
          bobGroup.group.asExternalGroup(),
          toVerifiedEntry(updatedGroup.entry)));

      CHECK_EQ(resultGroup, Group{updatedGroup.group.tankerGroup});
    }
  }
}
