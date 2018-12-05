#pragma once

#include <Tanker/BlockGenerator.hpp>
#include <Tanker/Client.hpp>
#include <Tanker/Groups/Group.hpp>
#include <Tanker/Groups/GroupStore.hpp>
#include <Tanker/Types/SGroupId.hpp>
#include <Tanker/UserAccessor.hpp>

#include <tconcurrent/coroutine.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace Tanker
{
namespace Groups
{
namespace Manager
{
static constexpr size_t MAX_GROUP_SIZE = 1000;

tc::cotask<std::vector<Crypto::PublicEncryptionKey>> getMemberKeys(
    UserAccessor& userAccessor, std::vector<UserId> const& memberUserIds);

tc::cotask<std::vector<uint8_t>> generateCreateGroupBlock(
    std::vector<Crypto::PublicEncryptionKey> const& memberUserKeys,
    BlockGenerator const& blockGenerator,
    Crypto::SignatureKeyPair const& groupSignatureKey,
    Crypto::EncryptionKeyPair const& groupEncryptionKey);

tc::cotask<SGroupId> create(UserAccessor& userAccessor,
                            BlockGenerator const& blockGenerator,
                            Client& client,
                            std::vector<UserId> const& members);

tc::cotask<std::vector<uint8_t>> generateAddUserToGroupBlock(
    std::vector<Crypto::PublicEncryptionKey> const& memberUserKeys,
    BlockGenerator const& blockGenerator,
    Group const& group);

tc::cotask<void> updateMembers(UserAccessor& userAccessor,
                               BlockGenerator const& blockGenerator,
                               Client& client,
                               GroupStore const& groupStore,
                               GroupId const& groupId,
                               std::vector<UserId> const& usersToAdd);
}
}
}
