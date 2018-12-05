#pragma once

#include <Tanker/Crypto/Types.hpp>
#include <Tanker/Device.hpp>
#include <Tanker/Entry.hpp>
#include <Tanker/ResourceKeyStore.hpp>
#include <Tanker/Trustchain.hpp>
#include <Tanker/Types/DeviceId.hpp>
#include <Tanker/Types/UserId.hpp>

#include <gsl-lite.hpp>
#include <tconcurrent/coroutine.hpp>

#include <cstdint>
#include <tuple>
#include <vector>

namespace Tanker
{
class BlockGenerator;
class UserAccessor;
class GroupAccessor;
class Client;

namespace Share
{
using ResourceKey = std::tuple<Crypto::SymmetricKey, Crypto::Mac>;
using ResourceKeys = std::vector<ResourceKey>;

struct KeyRecipients
{
  std::vector<Crypto::PublicEncryptionKey> recipientUserKeys;
  std::vector<Crypto::PublicEncryptionKey> recipientGroupKeys;
  std::vector<Device> recipientDevices;
};

std::vector<uint8_t> makeKeyPublishToDevice(
    BlockGenerator const& blockGenerator,
    Crypto::PrivateEncryptionKey const& selfPrivateEncryptionKey,
    DeviceId const& recipientDeviceId,
    Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey,
    Crypto::Mac const& resourceId,
    Crypto::SymmetricKey const& resourceKey);

std::vector<uint8_t> makeKeyPublishToDeviceToUser(
    BlockGenerator const& blockGenerator,
    Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey,
    Crypto::Mac const& resourceId,
    Crypto::SymmetricKey const& resourceKey);

std::vector<uint8_t> makeKeyPublishToDeviceToGroup(
    BlockGenerator const& blockGenerator,
    Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey,
    Crypto::Mac const& resourceId,
    Crypto::SymmetricKey const& resourceKey);

tc::cotask<KeyRecipients> generateRecipientList(
    UserAccessor& userAccessor,
    GroupAccessor& groupAccessor,
    std::vector<UserId> const& userIds,
    std::vector<GroupId> const& groupIds);

std::vector<std::vector<uint8_t>> generateShareBlocks(
    Crypto::PrivateEncryptionKey const& selfPrivateEncryptionKey,
    BlockGenerator const& blockGenerator,
    ResourceKeys const& resourceKeys,
    KeyRecipients const& keyRecipients);

tc::cotask<void> share(
    Crypto::PrivateEncryptionKey const& selfPrivateEncryptionKey,
    ResourceKeyStore const& resourceKeyStore,
    UserAccessor& userAccessor,
    GroupAccessor& groupAccessor,
    BlockGenerator const& blockGenerator,
    Client& client,
    std::vector<Crypto::Mac> const& resourceIds,
    std::vector<UserId> const& recipientUserIds,
    std::vector<GroupId> const& groupIds);
}
}
