#include <Tanker/Groups/GroupUpdater.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Crypto/Format/Format.hpp>
#include <Tanker/Error.hpp>
#include <Tanker/Format/Enum.hpp>
#include <Tanker/Trustchain/GroupId.hpp>

#include <Tanker/Log.hpp>

TLOG_CATEGORY(GroupUpdater);

using Tanker::Trustchain::GroupId;
using namespace Tanker::Trustchain::Actions;

namespace Tanker
{
namespace GroupUpdater
{
namespace
{
tc::cotask<nonstd::optional<Crypto::PrivateEncryptionKey>> decryptMyKey(
    UserKeyStore const& userKeyStore,
    UserGroupCreation::v1::SealedPrivateEncryptionKeysForUsers const& groupKeys)
{
  for (auto const& gek : groupKeys)
  {
    if (auto const matchingUserKeyPair =
            TC_AWAIT(userKeyStore.findKeyPair(gek.first)))
    {
      auto const groupPrivateEncryptionKey =
          Crypto::sealDecrypt(gek.second, *matchingUserKeyPair);
      TC_RETURN(groupPrivateEncryptionKey);
    }
  }
  TC_RETURN(nonstd::nullopt);
}

tc::cotask<nonstd::optional<Crypto::PrivateEncryptionKey>> decryptMyKey(
    Trustchain::UserId const& myUserId,
    UserKeyStore const& userKeyStore,
    UserGroupCreation::v2::Members const& groupKeys)
{
  auto const myKeysIt =
      std::find_if(groupKeys.begin(), groupKeys.end(), [&](auto const& k) {
        return k.userId() == myUserId;
      });
  if (myKeysIt == groupKeys.end())
    TC_RETURN(nonstd::nullopt);

  auto const userKeyPair =
      TC_AWAIT(userKeyStore.findKeyPair(myKeysIt->userPublicKey()));
  if (!userKeyPair)
    throw std::runtime_error(
        "assertion error: group block does contains my user id but not my user "
        "key");

  auto const groupPrivateEncryptionKey = Crypto::sealDecrypt(
      myKeysIt->encryptedPrivateEncryptionKey(), *userKeyPair);
  TC_RETURN(groupPrivateEncryptionKey);
}

tc::cotask<nonstd::optional<Crypto::PrivateEncryptionKey>>
decryptMyProvisionalKey(
    ProvisionalUserKeysStore const& provisionalUserKeysStore,
    UserGroupCreation::v2::ProvisionalMembers const& groupKeys)
{
  for (auto const& gek : groupKeys)
  {
    if (auto const matchingProvisionalUserKeys =
            TC_AWAIT(provisionalUserKeysStore.findProvisionalUserKeys(
                gek.appPublicSignatureKey(), gek.tankerPublicSignatureKey())))
    {
      auto const groupPrivateEncryptionKey = Crypto::sealDecrypt(
          Crypto::sealDecrypt(gek.encryptedPrivateEncryptionKey(),
                              matchingProvisionalUserKeys->tankerKeys),
          matchingProvisionalUserKeys->appKeys);
      TC_RETURN(groupPrivateEncryptionKey);
    }
  }
  TC_RETURN(nonstd::nullopt);
}

std::vector<GroupProvisionalUser> extractGroupProvisionalUsers(
    std::vector<UserGroupProvisionalMember2> const& members)
{
  std::vector<GroupProvisionalUser> out;
  out.reserve(members.size());
  for (auto const& member : members)
    out.push_back({member.appPublicSignatureKey(),
                   member.tankerPublicSignatureKey(),
                   member.encryptedPrivateEncryptionKey()});
  return out;
}

std::vector<GroupProvisionalUser> extractGroupProvisionalUsers(
    UserGroupCreation const& g)
{
  if (auto const g2 = g.get_if<UserGroupCreation::v2>())
    return extractGroupProvisionalUsers(g2->provisionalMembers());
  return {};
}

std::vector<GroupProvisionalUser> extractGroupProvisionalUsers(
    UserGroupAddition const& g)
{
  if (auto const g2 = g.get_if<UserGroupAddition::v2>())
    return extractGroupProvisionalUsers(g2->provisionalMembers());
  return {};
}

tc::cotask<void> putExternalGroup(GroupStore& groupStore,
                                  Entry const& entry,
                                  UserGroupCreation const& userGroupCreation)
{
  TC_AWAIT(groupStore.put(ExternalGroup{
      GroupId{userGroupCreation.publicSignatureKey()},
      userGroupCreation.publicSignatureKey(),
      userGroupCreation.sealedPrivateSignatureKey(),
      userGroupCreation.publicEncryptionKey(),
      entry.hash,
      entry.index,
      extractGroupProvisionalUsers(userGroupCreation),
  }));
}

tc::cotask<void> putFullGroup(
    GroupStore& groupStore,
    Crypto::PrivateEncryptionKey const& groupPrivateEncryptionKey,
    Entry const& entry,
    UserGroupCreation const& userGroupCreation)
{
  auto const groupPrivateSignatureKey =
      Crypto::sealDecrypt(userGroupCreation.sealedPrivateSignatureKey(),
                          Crypto::EncryptionKeyPair{
                              userGroupCreation.publicEncryptionKey(),
                              groupPrivateEncryptionKey,
                          });
  TC_AWAIT(groupStore.put(Group{
      GroupId{userGroupCreation.publicSignatureKey()},
      Crypto::SignatureKeyPair{
          userGroupCreation.publicSignatureKey(),
          groupPrivateSignatureKey,
      },
      Crypto::EncryptionKeyPair{
          userGroupCreation.publicEncryptionKey(),
          groupPrivateEncryptionKey,
      },
      entry.hash,
      entry.index,
  }));
}

tc::cotask<void> putFullGroup(
    GroupStore& groupStore,
    ExternalGroup const& previousGroup,
    Crypto::PrivateEncryptionKey const& groupPrivateEncryptionKey,
    Entry const& entry)
{
  auto const groupPrivateSignatureKey =
      Crypto::sealDecrypt(*previousGroup.encryptedPrivateSignatureKey,
                          Crypto::EncryptionKeyPair{
                              previousGroup.publicEncryptionKey,
                              groupPrivateEncryptionKey,
                          });
  TC_AWAIT(groupStore.put(Group{
      GroupId{previousGroup.publicSignatureKey},
      Crypto::SignatureKeyPair{
          previousGroup.publicSignatureKey,
          groupPrivateSignatureKey,
      },
      Crypto::EncryptionKeyPair{
          previousGroup.publicEncryptionKey,
          groupPrivateEncryptionKey,
      },
      entry.hash,
      entry.index,
  }));
}

tc::cotask<void> applyUserGroupCreation(
    Trustchain::UserId const& myUserId,
    GroupStore& groupStore,
    UserKeyStore const& userKeyStore,
    ProvisionalUserKeysStore const& provisionalUserKeysStore,
    Entry const& entry)
{
  auto const& userGroupCreation = entry.action.get<UserGroupCreation>();

  nonstd::optional<Crypto::PrivateEncryptionKey> groupPrivateEncryptionKey;
  if (auto const ugc1 = userGroupCreation.get_if<UserGroupCreation::v1>())
    groupPrivateEncryptionKey = TC_AWAIT(decryptMyKey(
        userKeyStore, ugc1->sealedPrivateEncryptionKeysForUsers()));
  else if (auto const ugc2 = userGroupCreation.get_if<UserGroupCreation::v2>())
  {
    groupPrivateEncryptionKey = TC_AWAIT(
        decryptMyKey(myUserId, userKeyStore, ugc2->members()));
    if (!groupPrivateEncryptionKey)
      groupPrivateEncryptionKey = TC_AWAIT(decryptMyProvisionalKey(
          provisionalUserKeysStore, ugc2->provisionalMembers()));
  }

  if (groupPrivateEncryptionKey)
    TC_AWAIT(putFullGroup(
        groupStore, *groupPrivateEncryptionKey, entry, userGroupCreation));
  else
    TC_AWAIT(putExternalGroup(groupStore, entry, userGroupCreation));
}

tc::cotask<void> applyUserGroupAddition(
    Trustchain::UserId const& myUserId,
    GroupStore& groupStore,
    UserKeyStore const& userKeyStore,
    ProvisionalUserKeysStore const& provisionalUserKeysStore,
    Entry const& entry)
{
  auto const& userGroupAddition = entry.action.get<UserGroupAddition>();

  auto const previousGroup =
      TC_AWAIT(groupStore.findExternalById(userGroupAddition.groupId()));
  if (!previousGroup)
    throw Error::formatEx<std::runtime_error>(
        "assertion error: can't find previous group block for {}",
        userGroupAddition.groupId());

  TC_AWAIT(groupStore.updateLastGroupBlock(
      userGroupAddition.groupId(), entry.hash, entry.index));

  nonstd::optional<Crypto::PrivateEncryptionKey> groupPrivateEncryptionKey;
  if (auto const uga1 = userGroupAddition.get_if<UserGroupAddition::v1>())
    groupPrivateEncryptionKey = TC_AWAIT(decryptMyKey(
        userKeyStore, uga1->sealedPrivateEncryptionKeysForUsers()));
  else if (auto const uga2 = userGroupAddition.get_if<UserGroupAddition::v2>())
  {
    groupPrivateEncryptionKey =
        TC_AWAIT(decryptMyKey(myUserId, userKeyStore, uga2->members()));
    if (!groupPrivateEncryptionKey)
      groupPrivateEncryptionKey = TC_AWAIT(decryptMyProvisionalKey(
          provisionalUserKeysStore, uga2->provisionalMembers()));
  }

  // I am already member of this group, ignore
  if (!previousGroup->encryptedPrivateSignatureKey)
    TC_RETURN();
  // I am still not part of this group, store provisional members for maybe
  // future use
  if (!groupPrivateEncryptionKey)
  {
    TC_AWAIT(groupStore.putGroupProvisionalEncryptionKeys(
        userGroupAddition.groupId(),
        extractGroupProvisionalUsers(userGroupAddition)));
    TC_RETURN();
  }

  TC_AWAIT(putFullGroup(
      groupStore, *previousGroup, *groupPrivateEncryptionKey, entry));
}
}

tc::cotask<void> applyEntry(
    Trustchain::UserId const& myUserId,
    GroupStore& groupStore,
    UserKeyStore const& userKeyStore,
    ProvisionalUserKeysStore const& provisionalUserKeysStore,
    Entry const& entry)
{
  if (entry.action.holdsAlternative<UserGroupCreation>())
    TC_AWAIT(applyUserGroupCreation(
        myUserId, groupStore, userKeyStore, provisionalUserKeysStore, entry));
  else if (entry.action.holdsAlternative<UserGroupAddition>())
    TC_AWAIT(applyUserGroupAddition(
        myUserId, groupStore, userKeyStore, provisionalUserKeysStore, entry));
  else
    throw Error::formatEx<std::runtime_error>(
        "GroupUpdater can't handle this block (nature: {})", entry.nature);
}

tc::cotask<void> applyGroupPrivateKey(
    GroupStore& groupStore,
    ExternalGroup const& group,
    Crypto::PrivateEncryptionKey const& groupPrivateEncryptionKey)
{
  if (!group.encryptedPrivateSignatureKey)
    // we are already in the group, nothing more to decrypt
    TC_RETURN();

  auto const groupPrivateSignatureKey =
      Crypto::sealDecrypt(*group.encryptedPrivateSignatureKey,
                          Crypto::EncryptionKeyPair{
                              group.publicEncryptionKey,
                              groupPrivateEncryptionKey,
                          });
  TC_AWAIT(groupStore.put(Group{
      group.id,
      Crypto::SignatureKeyPair{
          group.publicSignatureKey,
          groupPrivateSignatureKey,
      },
      Crypto::EncryptionKeyPair{
          group.publicEncryptionKey,
          groupPrivateEncryptionKey,
      },
      group.lastBlockHash,
      group.lastBlockIndex,
  }));
}
}
}
