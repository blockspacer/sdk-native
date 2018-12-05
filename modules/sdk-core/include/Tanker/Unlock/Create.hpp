#pragma once

#include <Tanker/Crypto/Types.hpp>
#include <Tanker/DeviceKeys.hpp>
#include <Tanker/GhostDevice.hpp>
#include <Tanker/Types/TrustchainId.hpp>
#include <Tanker/Types/UnlockKey.hpp>
#include <Tanker/Types/UserId.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Tanker
{
class BlockGenerator;
struct EncryptedUserKey;

namespace Unlock
{
struct Registration;

UnlockKey ghostDeviceToUnlockKey(GhostDevice const& ghostDevice);

std::unique_ptr<Registration> generate(
    UserId const& userId,
    Crypto::EncryptionKeyPair const& userKeypair,
    BlockGenerator const& blockGen,
    DeviceKeys const& deviceKeys = DeviceKeys::create());

GhostDevice extract(UnlockKey const& unlockKey);

std::vector<uint8_t> createValidatedDevice(
    TrustchainId const& trustchainId,
    UserId const& userId,
    GhostDevice const& ghostDevice,
    DeviceKeys const& deviceKeys,
    EncryptedUserKey const& encryptedUserKey);
}
}
