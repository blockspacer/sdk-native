#include <Tanker/Error.hpp>
#include <Tanker/Identity/Delegation.hpp>
#include <Tanker/Serialization/Serialization.hpp>
#include <Tanker/Trustchain/Action.hpp>
#include <Tanker/Trustchain/Actions/DeviceCreation.hpp>
#include <Tanker/Trustchain/Actions/KeyPublishToProvisionalUser.hpp>
#include <Tanker/Trustchain/Actions/KeyPublishToUserGroup.hpp>
#include <Tanker/Trustchain/Actions/TrustchainCreation.hpp>
#include <Tanker/Trustchain/ServerEntry.hpp>
#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/Verif/DeviceCreation.hpp>
#include <Tanker/Verif/DeviceRevocation.hpp>
#include <Tanker/Verif/KeyPublishToDevice.hpp>
#include <Tanker/Verif/KeyPublishToUser.hpp>
#include <Tanker/Verif/KeyPublishToUserGroup.hpp>
#include <Tanker/Verif/ProvisionalIdentityClaim.hpp>
#include <Tanker/Verif/TrustchainCreation.hpp>
#include <Tanker/Verif/UserGroupAddition.hpp>
#include <Tanker/Verif/UserGroupCreation.hpp>

#include <Helpers/Buffers.hpp>

#include <doctest.h>

#include <cstring>

#include "TestVerifier.hpp"
#include "TrustchainBuilder.hpp"

using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;
using namespace Tanker;

#define CHECK_VERIFICATION_FAILED_WITH(functionCall, errCode) \
  do                                                          \
  {                                                           \
    try                                                       \
    {                                                         \
      functionCall;                                           \
    }                                                         \
    catch (Error::VerificationFailed const& err)              \
    {                                                         \
      CHECK_EQ(err.code(), errCode);                          \
      break;                                                  \
    }                                                         \
    catch (...)                                               \
    {                                                         \
    }                                                         \
    REQUIRE(false);                                           \
  } while (0)

namespace
{
template <typename T, typename U>
T& extract(U& action)
{
  return const_cast<T&>(action.template get<T>());
}

template <typename T, typename U, typename V>
U& unconstify(T& action, U const& (V::*method)() const)
{
  auto const& subAction = action.template get<V>();
  return const_cast<U&>((subAction.*method)());
}

template <typename T, typename U>
U& unconstify(T& action, U const& (T::*method)() const)
{
  return const_cast<U&>((action.*method)());
}

template <typename T>
void alter(ServerEntry& entry, T const& (ServerEntry::*method)() const)
{
  ++unconstify(entry, method)[0];
}

template <typename T, typename U, typename V>
void alter(T& action, U const& (V::*method)() const)
{
  ++unconstify(action, method)[0];
}

template <typename T, typename U>
void alter(T& action, U const& (T::*method)() const)
{
  ++unconstify(action, method)[0];
}

template <typename T>
void deviceCreationCommonChecks(TrustchainBuilder::ResultUser user,
                                TrustchainCreation const& trustchainCreation)
{
  SUBCASE("it should reject an incorrectly signed delegation for a device")
  {
    auto& deviceCreation = extract<DeviceCreation>(user.entry.action());
    alter(deviceCreation, &DeviceCreation::delegationSignature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(user.entry, trustchainCreation),
        Error::VerificationCode::InvalidDelegationSignature);
  }

  SUBCASE("should reject an incorrectly signed device")
  {
    alter(user.entry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(user.entry, trustchainCreation),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should accept a valid DeviceCreation")
  {
    CHECK_NOTHROW(Verif::verifyDeviceCreation(user.entry, trustchainCreation));
  }
}

template <typename T>
void deviceCreationCommonChecks(TrustchainBuilder::ResultUser user,
                                Device authorDevice,
                                TrustchainBuilder::ResultDevice secondDevice)
{
  auto const tankerUser = user.user.asTankerUser();

  SUBCASE("it should reject a device creation when author device is revoked")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("it should reject an incorrectly signed delegation for a device")
  {
    auto& deviceCreation = extract<DeviceCreation>(secondDevice.entry.action());
    alter(deviceCreation, &DeviceCreation::delegationSignature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidDelegationSignature);
  }

  SUBCASE("should reject an incorrectly signed device")
  {
    alter(secondDevice.entry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject an incorrect userId")
  {
    auto& deviceCreation = extract<DeviceCreation>(secondDevice.entry.action());
    alter(deviceCreation, &DeviceCreation::userId);
    auto const& authorPrivateSignatureKey =
        user.user.devices.front().keys.signatureKeyPair.privateKey;
    deviceCreation.sign(authorPrivateSignatureKey);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidUserId);
  }

  SUBCASE("should accept a valid DeviceCreation")
  {
    CHECK_NOTHROW(Verif::verifyDeviceCreation(
        secondDevice.entry, authorDevice, tankerUser));
  }
}

void deviceRevocationCommonChecks(ServerEntry deviceRevocation,
                                  Device authorDevice,
                                  Device targetDevice,
                                  User const& user)
{
  SUBCASE("should reject an incorrectly signed DeviceRevocation")
  {
    alter(deviceRevocation, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            deviceRevocation, authorDevice, targetDevice, user),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a revocation from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            deviceRevocation, authorDevice, targetDevice, user),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should reject a revocation of an already revoked device")
  {
    targetDevice.revokedAtBlkIndex = targetDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            deviceRevocation, authorDevice, targetDevice, user),
        Error::VerificationCode::InvalidTargetDevice);
  }

  SUBCASE("should accept a valid deviceRevocation")
  {
    CHECK_NOTHROW(Verif::verifyDeviceRevocation(
        deviceRevocation, authorDevice, targetDevice, user));
  }
}
}

TEST_CASE("Verif TrustchainCreation")
{
  TrustchainBuilder builder;
  auto rootEntry = blockToServerEntry(builder.blocks().front());

  SUBCASE("Entry level")
  {
    SUBCASE("Invalid author")
    {
      alter(rootEntry, &ServerEntry::author);

      CHECK_VERIFICATION_FAILED_WITH(
          Verif::verifyTrustchainCreation(rootEntry, builder.trustchainId()),
          Error::VerificationCode::InvalidAuthor);
    }

    SUBCASE("Invalid signature")
    {
      alter(rootEntry, &ServerEntry::signature);

      CHECK_VERIFICATION_FAILED_WITH(
          Verif::verifyTrustchainCreation(rootEntry, builder.trustchainId()),
          Error::VerificationCode::InvalidSignature);
    }

    SUBCASE("Valid TrustchainCreation block")
    {
      CHECK_NOTHROW(
          Verif::verifyTrustchainCreation(rootEntry, builder.trustchainId()));
    }
  }

  SUBCASE("Contextual level")
  {
    SUBCASE("TrustchainId mismatch")
    {
      Trustchain::TrustchainId trustchainId(rootEntry.hash());
      trustchainId[0]++;

      CHECK_VERIFICATION_FAILED_WITH(
          Verif::verifyTrustchainCreation(rootEntry, trustchainId),
          Error::VerificationCode::InvalidHash);
    }

    SUBCASE("Valid TrustchainCreation block")
    {
      Trustchain::TrustchainId trustchainId(rootEntry.hash());

      CHECK_NOTHROW(Verif::verifyTrustchainCreation(rootEntry, trustchainId));
    }
  }
}

TEST_CASE("Verif DeviceCreation v3 - Trustchain author")
{
  TrustchainBuilder builder;

  auto user = builder.makeUser3("alice");

  auto const authorEntry =
      toVerifiedEntry(blockToServerEntry(builder.blocks()[0]));
  auto const trustchainCreation = authorEntry.action.get<TrustchainCreation>();

  deviceCreationCommonChecks<Trustchain::Actions::DeviceCreation::v3>(
      user, trustchainCreation);
}

TEST_CASE("Verif DeviceCreation v1 - Trustchain author")
{
  TrustchainBuilder builder;

  auto user = builder.makeUser1("alice");

  auto const authorEntry =
      toVerifiedEntry(blockToServerEntry(builder.blocks()[0]));
  auto const trustchainCreation = authorEntry.action.get<TrustchainCreation>();

  deviceCreationCommonChecks<Trustchain::Actions::DeviceCreation::v1>(
      user, trustchainCreation);
}

TEST_CASE("Verif DeviceCreation v3 - DeviceCreation v3 author")
{
  TrustchainBuilder builder;

  auto user = builder.makeUser3("alice");
  auto secondDevice = builder.makeDevice3("alice");

  auto const authorDevice = user.user.devices.front().asTankerDevice();

  deviceCreationCommonChecks<Trustchain::Actions::DeviceCreation::v3>(
      user, authorDevice, secondDevice);

  SUBCASE("should reject an incorrect userKey")
  {
    auto& deviceCreation = extract<DeviceCreation>(secondDevice.entry.action());

    alter(deviceCreation, &DeviceCreation::v3::publicUserEncryptionKey);
    deviceCreation.sign(
        user.user.devices.front().keys.signatureKeyPair.privateKey);

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, user.user.asTankerUser()),
        Error::VerificationCode::InvalidUserKey);
  }
}

TEST_CASE("Verif DeviceCreation v3 - DeviceCreation v1 author")
{
  TrustchainBuilder builder;

  auto const user = builder.makeUser1("alice");
  auto secondDevice = builder.makeDevice3("alice");
  auto const authorDevice = user.user.devices.front().asTankerDevice();
  auto tankerUser = user.user.asTankerUser();

  SUBCASE("should reject a device creation 3 if the user has no user key")
  {
    tankerUser.userKey = nonstd::nullopt;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidUserKey);
  }

  SUBCASE("should reject an incorrect userKey")
  {
    auto& deviceCreation = extract<DeviceCreation>(secondDevice.entry.action());

    alter(deviceCreation, &DeviceCreation::v3::publicUserEncryptionKey);
    deviceCreation.sign(
        user.user.devices.front().keys.signatureKeyPair.privateKey);

    tankerUser.userKey = secondDevice.user.userKey;

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidUserKey);
  }
}

TEST_CASE("Verif DeviceCreation v1 - DeviceCreation v1 author")
{
  TrustchainBuilder builder;

  auto alice = builder.makeUser1("alice");
  auto secondDevice = builder.makeDevice1("alice");
  auto const authorDevice = alice.user.devices.front().asTankerDevice();

  deviceCreationCommonChecks<Trustchain::Actions::DeviceCreation::v1>(
      alice, authorDevice, secondDevice);

  SUBCASE("should reject a device creation v1 if the user has a userKey")
  {
    auto const keyPair = Crypto::makeEncryptionKeyPair();
    auto tankerUser = alice.user.asTankerUser();
    tankerUser.userKey = keyPair.publicKey;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceCreation(
            secondDevice.entry, authorDevice, tankerUser),
        Error::VerificationCode::InvalidUserKey);
  }
}

TEST_CASE("KeyPublishToDevice")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser1("alice");
  auto const secondDevice = builder.makeDevice1("alice");
  auto const resourceId = make<Trustchain::ResourceId>("resourceId");
  auto const symmetricKey = make<Crypto::SymmetricKey>("symmetric key");
  auto const kp2d = builder.shareToDevice(
      secondDevice.device, resultUser.user, resourceId, symmetricKey);
  auto kp2dEntry = blockToServerEntry(kp2d.front());
  auto const targetUser = secondDevice.user;
  auto authorDevice = secondDevice.device.asTankerDevice();

  SUBCASE("should reject an incorrectly signed KeyPublishToDevice")
  {
    alter(kp2dEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToDevice(kp2dEntry, authorDevice, targetUser),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a keyPublishToDevice to a recipient with a user key")
  {
    auto const deviceV3 = builder.makeDevice3("alice");
    auto const targetUserUpdated = deviceV3.user;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToDevice(
            kp2dEntry, authorDevice, targetUserUpdated),
        Error::VerificationCode::InvalidUserKey);
  }

  SUBCASE("should reject a KeyPublishToDevice from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToDevice(kp2dEntry, authorDevice, targetUser),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid KeyPublishToDevice")
  {
    CHECK_NOTHROW(
        Verif::verifyKeyPublishToDevice(kp2dEntry, authorDevice, targetUser));
  }
}

TEST_CASE("KeyPublishToUser")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser3("alice");
  auto const secondDevice = builder.makeDevice3("alice");
  auto const resourceId = make<Trustchain::ResourceId>("resourceId");
  auto const symmetricKey = make<Crypto::SymmetricKey>("symmetric key");
  auto const kp2u = builder.shareToUser(
      secondDevice.device, resultUser.user, resourceId, symmetricKey);
  auto kp2uEntry = blockToServerEntry(kp2u);
  auto authorDevice = secondDevice.device.asTankerDevice();

  SUBCASE("Should reject an incorrectly signed KeyPublishToUser")
  {
    alter(kp2uEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToUser(kp2uEntry, authorDevice),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a KeyPublishToUser from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToUser(kp2uEntry, authorDevice),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid KeyPublishToUser")
  {
    CHECK_NOTHROW(Verif::verifyKeyPublishToUser(kp2uEntry, authorDevice));
  }
}

TEST_CASE("KeyPublishToProvisionalUser")
{
  TrustchainBuilder builder;

  auto const user = builder.makeUser3("alice");
  auto const device = user.user.devices.front();

  auto const appPublicSignatureKey =
      make<Crypto::PublicSignatureKey>("app sig key");
  auto const tankerPublicSignatureKey =
      make<Crypto::PublicSignatureKey>("tanker sig key");
  auto const resourceId = make<Trustchain::ResourceId>("resourceId");
  auto const twoTimesSealedSymmetricKey =
      make<Crypto::TwoTimesSealedSymmetricKey>(
          "two times sealed symmetric key");
  auto const blockGenerator = builder.makeBlockGenerator(device);
  auto const block = Serialization::deserialize<Block>(
      blockGenerator.keyPublishToProvisionalUser(appPublicSignatureKey,
                                                 tankerPublicSignatureKey,
                                                 resourceId,
                                                 twoTimesSealedSymmetricKey));
  auto authorDevice = device.asTankerDevice();

  auto kp2puEntry = blockToServerEntry(block);
  unconstify(kp2puEntry, &ServerEntry::index) = 3;

  SUBCASE("Should reject an incorrectly signed KeyPublishToUser")
  {
    alter(kp2puEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToUser(kp2puEntry, authorDevice),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a KeyPublishToUser from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyKeyPublishToUser(kp2puEntry, authorDevice),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid KeyPublishToUser")
  {
    CHECK_NOTHROW(Verif::verifyKeyPublishToUser(kp2puEntry, authorDevice));
  }
}

TEST_CASE("KeyPublishToUserGroups")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser3("alice");
  auto const secondDevice = builder.makeDevice3("alice");
  auto const resourceId = make<Trustchain::ResourceId>("resourceId");
  auto const symmetricKey = make<Crypto::SymmetricKey>("symmetric key");
  auto const userBob = builder.makeUser3("bob");
  auto const resultGroup =
      builder.makeGroup(secondDevice.device, {resultUser.user, userBob.user});

  auto const kp2g = builder.shareToUserGroup(
      secondDevice.device, resultGroup.group, resourceId, symmetricKey);
  auto kp2gEntry = blockToServerEntry(kp2g);
  auto const targetGroup = resultGroup.group.tankerGroup;
  auto authorDevice = secondDevice.device.asTankerDevice();

  SUBCASE("should reject an incorrecly signed KeyPublishToUserGroups")
  {
    alter(kp2gEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyKeyPublishToUserGroup(
                                       kp2gEntry, authorDevice, targetGroup),
                                   Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a KeyPublishToUserGroup from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyKeyPublishToUserGroup(
                                       kp2gEntry, authorDevice, targetGroup),
                                   Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid KeyPublishToUserGroups")
  {
    CHECK_NOTHROW(Verif::verifyKeyPublishToUserGroup(
        kp2gEntry, authorDevice, targetGroup));
  }
}

TEST_CASE("Verif DeviceRevocationV1")
{
  TrustchainBuilder builder;

  auto user = builder.makeUser1("alice");
  auto secondDevice = builder.makeDevice1("alice");
  auto thirdDevice = builder.makeDevice1("alice");
  auto const authorDevice = secondDevice.device.asTankerDevice();
  auto const targetDevice = thirdDevice.device.asTankerDevice();

  auto const revokeBlock =
      builder.revokeDevice1(secondDevice.device, thirdDevice.device);

  deviceRevocationCommonChecks(blockToServerEntry(revokeBlock),
                               authorDevice,
                               targetDevice,
                               thirdDevice.user);

  SUBCASE("should reject a revocation for another user's device")
  {
    auto bob = builder.makeUser1("bob");
    auto bobDevice = builder.makeDevice1("bob");

    auto const revokeBobBlock =
        builder.revokeDevice1(secondDevice.device, bobDevice.device, true);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(blockToServerEntry(revokeBobBlock),
                                      authorDevice,
                                      bobDevice.device.asTankerDevice(),
                                      secondDevice.user),
        Error::VerificationCode::InvalidUser);
  }

  SUBCASE("should reject a revocation whose user has a userKey v1")
  {
    auto fourthDevice = builder.makeDevice3("alice");
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(blockToServerEntry(revokeBlock),
                                      authorDevice,
                                      targetDevice,
                                      fourthDevice.user),
        Error::VerificationCode::InvalidUserKey);
  }
}

TEST_CASE("Verif DeviceRevocationV2")
{
  TrustchainBuilder builder;

  auto user = builder.makeUser3("alice");
  auto secondDevice = builder.makeDevice3("alice");
  auto thirdDevice = builder.makeDevice3("alice");
  auto authorDevice = secondDevice.device.asTankerDevice();
  auto targetDevice = thirdDevice.device.asTankerDevice();
  auto aliceUser = builder.getUser("alice");
  auto const revokeBlock = builder.revokeDevice2(
      secondDevice.device, thirdDevice.device, *aliceUser);
  auto entry = blockToServerEntry(revokeBlock);

  auto bob = builder.makeUser1("bob");
  auto bobDevice = builder.makeDevice1("bob");
  auto bobOtherDevice = builder.makeDevice1("bob");
  auto const authorDeviceV1 = bobDevice.device.asTankerDevice();
  auto const targetDeviceV1 = bobOtherDevice.device.asTankerDevice();
  auto bobUser = builder.getUser("bob");
  auto const revokeBlockUserV1 =
      builder.revokeDevice2(bobDevice.device, bobOtherDevice.device, *bobUser);
  auto entryUserV1 = blockToServerEntry(revokeBlockUserV1);

  deviceRevocationCommonChecks(blockToServerEntry(revokeBlock),
                               authorDevice,
                               targetDevice,
                               thirdDevice.user);

  SUBCASE(
      "should reject a revocation whose user has no userKey when "
      "PreviousPublicEncryptionKey is not a zero array")
  {
    auto& dr = extract<DeviceRevocation>(entryUserV1.action());
    alter(dr, &DeviceRevocation2::previousPublicEncryptionKey);

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entryUserV1, authorDeviceV1, targetDeviceV1, bobOtherDevice.user),
        Error::VerificationCode::InvalidEncryptionKey);
  }

  SUBCASE(
      "should reject a revocation whose user has no userKey when the "
      "EncryptedKeyForPreviousUserKey is not a zero array")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entryUserV1.action());
    alter(deviceRevocation, &DeviceRevocation2::sealedKeyForPreviousUserKey);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entryUserV1, authorDeviceV1, targetDeviceV1, bobOtherDevice.user),
        Error::VerificationCode::InvalidUserKey);
  }

  SUBCASE("should reject a revocation for another user's device")
  {
    auto const revokeBlock = builder.revokeDevice2(
        secondDevice.device, bobOtherDevice.device, user.user, true);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(blockToServerEntry(revokeBlock),
                                      authorDevice,
                                      targetDeviceV1,
                                      secondDevice.user),
        Error::VerificationCode::InvalidUser);
  }

  SUBCASE(
      "should reject a revocation whose user has a userKey when the "
      "previousEncryptedKey does not match the userKey")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entry.action());
    alter(deviceRevocation, &DeviceRevocation2::previousPublicEncryptionKey);

    REQUIRE_FALSE(authorDevice.revokedAtBlkIndex.has_value());
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entry, authorDevice, targetDevice, thirdDevice.user),
        Error::VerificationCode::InvalidEncryptionKey);
  }

  SUBCASE(
      "should reject a DeviceRevocation2 whose userKeys field does not have "
      "exactly one element per device")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entry.action());
    auto& sealedUserKeysForDevices = unconstify(
        deviceRevocation, &DeviceRevocation2::sealedUserKeysForDevices);
    sealedUserKeysForDevices.erase(sealedUserKeysForDevices.begin());

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entry, authorDevice, targetDevice, thirdDevice.user),
        Error::VerificationCode::InvalidUserKeys);
  }

  SUBCASE(
      "should reject a DeviceCreationV2 with a userKey fields that contains "
      "the target device of the revocation")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entry.action());
    auto& sealedUserKeysForDevices = unconstify(
        deviceRevocation, &DeviceRevocation2::sealedUserKeysForDevices);
    sealedUserKeysForDevices.erase(sealedUserKeysForDevices.begin());
    auto const sealedPrivateEncryptionKey =
        make<Crypto::SealedPrivateEncryptionKey>("encrypted private key");
    sealedUserKeysForDevices.emplace_back(thirdDevice.device.keys.deviceId,
                                          sealedPrivateEncryptionKey);

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entry, authorDevice, targetDevice, thirdDevice.user),
        Error::VerificationCode::InvalidUserKeys);
  }

  SUBCASE(
      "should reject a DeviceRevocation whose userKeys fields has a device "
      "that does not belong to the author's devices")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entry.action());
    auto& sealedUserKeysForDevices = unconstify(
        deviceRevocation, &DeviceRevocation2::sealedUserKeysForDevices);
    sealedUserKeysForDevices.erase(sealedUserKeysForDevices.begin());

    auto const sealedPrivateEncryptionKey =
        make<Crypto::SealedPrivateEncryptionKey>("encrypted private key");
    sealedUserKeysForDevices.emplace_back(bobDevice.device.keys.deviceId,
                                          sealedPrivateEncryptionKey);

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entry, authorDevice, targetDevice, thirdDevice.user),
        Error::VerificationCode::InvalidUserKeys);
  }

  SUBCASE(
      "should reject a DeviceRevocation whose userKeys fields has a duplicates")
  {
    auto& deviceRevocation = extract<DeviceRevocation>(entry.action());
    auto& sealedUserKeysForDevices = unconstify(
        deviceRevocation, &DeviceRevocation2::sealedUserKeysForDevices);
    sealedUserKeysForDevices.erase(sealedUserKeysForDevices.begin());
    sealedUserKeysForDevices.push_back(*sealedUserKeysForDevices.begin());

    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyDeviceRevocation(
            entry, authorDevice, targetDevice, thirdDevice.user),
        Error::VerificationCode::InvalidUserKeys);
  }
}

namespace
{
void testUserGroupCreationCommon(Tanker::Device& authorDevice,
                                 ServerEntry& gcEntry)
{
  SUBCASE("should reject an incorrectly signed UserGroupCreation")
  {
    alter(gcEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupCreation(gcEntry, authorDevice),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a UserGroupCreation with invalid selfSignature")
  {
    auto& userGroupCreation = extract<UserGroupCreation>(gcEntry.action());
    alter(userGroupCreation, &UserGroupCreation::selfSignature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupCreation(gcEntry, authorDevice),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a UserGroupCreation from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupCreation(gcEntry, authorDevice),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid UserGroupCreation")
  {
    CHECK_NOTHROW(Verif::verifyUserGroupCreation(gcEntry, authorDevice));
  }
}
}

TEST_CASE("Verif UserGroupCreation")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser3("alice");
  auto const firstDevice = resultUser.user.devices.at(0);
  auto const resultGroup = builder.makeGroup(firstDevice, {resultUser.user});

  auto gcEntry = resultGroup.entry;
  auto authorDevice = firstDevice.asTankerDevice();

  testUserGroupCreationCommon(authorDevice, gcEntry);
}

TEST_CASE("Verif UserGroupCreation2")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser3("alice");
  auto const firstDevice = resultUser.user.devices.at(0);
  auto const provisionalUser = builder.makeProvisionalUser("bob@tanker");
  auto const resultGroup =
      builder.makeGroup2(firstDevice, {resultUser.user}, {provisionalUser});

  auto gcEntry = resultGroup.entry;
  auto authorDevice = firstDevice.asTankerDevice();

  testUserGroupCreationCommon(authorDevice, gcEntry);
}

TEST_CASE("Verif UserGroupAddition")
{
  TrustchainBuilder builder;

  auto const resultUser = builder.makeUser3("alice");
  auto const secondDevice = builder.makeDevice3("alice");
  auto const bobUser = builder.makeUser3("bob");
  auto const resultGroup =
      builder.makeGroup(secondDevice.device, {resultUser.user});
  auto const resultUserGroupAddition = builder.addUserToGroup(
      secondDevice.device, resultGroup.group, {bobUser.user});

  auto gaEntry = resultUserGroupAddition.entry;
  auto const& group = resultGroup.group.tankerGroup;
  auto authorDevice = secondDevice.device.asTankerDevice();

  SUBCASE("should reject an incorrectly signed UserGroupAddition")
  {
    alter(gaEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupAddition(gaEntry, authorDevice, group),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE(
      "should reject a UserGroupAddition where previousGroupBlock is not the "
      "hash of last modification")
  {
    auto& userGroupAddition = extract<UserGroupAddition>(gaEntry.action());
    alter(userGroupAddition, &UserGroupAddition::previousGroupBlockHash);
    userGroupAddition.selfSign(
        secondDevice.device.keys.signatureKeyPair.privateKey);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupAddition(gaEntry, authorDevice, group),
        Error::VerificationCode::InvalidGroup);
  }

  SUBCASE("should reject a UserGroupAddition with invalid selfSignature")
  {
    auto& userGroupAddition = extract<UserGroupAddition>(gaEntry.action());
    alter(userGroupAddition, &UserGroupAddition::selfSignature);
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupAddition(gaEntry, authorDevice, group),
        Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a UserGroupAddition from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(
        Verif::verifyUserGroupAddition(gaEntry, authorDevice, group),
        Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should accept a valid UserGroupAddition")
  {
    CHECK_NOTHROW(Verif::verifyUserGroupAddition(gaEntry, authorDevice, group));
  }
}

TEST_CASE("Verif ProvisionalIdentityClaim")
{
  TrustchainBuilder builder;

  auto const alice = builder.makeUser3("alice");
  auto const provisionalUser = builder.makeProvisionalUser("alice@email.com");
  auto picEntry = builder.claimProvisionalIdentity("alice", provisionalUser);

  auto authorDevice = alice.user.devices[0].asTankerDevice();
  auto authorUser = alice.user.asTankerUser();

  SUBCASE("should reject a ProvisionalIdentityClaim from a revoked device")
  {
    authorDevice.revokedAtBlkIndex = authorDevice.createdAtBlkIndex + 1;
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidAuthor);
  }

  SUBCASE("should reject an incorrectly signed ProvisionalIdentityClaim")
  {
    alter(picEntry, &ServerEntry::signature);
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a ProvisionalIdentityClaim with invalid app signature")
  {
    auto& provisionalIdentityClaim =
        extract<ProvisionalIdentityClaim>(picEntry.action());
    alter(provisionalIdentityClaim,
          &ProvisionalIdentityClaim::authorSignatureByAppKey);
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidSignature);
  }

  SUBCASE(
      "should reject a ProvisionalIdentityClaim with invalid tanker signature")
  {
    auto& provisionalIdentityClaim =
        extract<ProvisionalIdentityClaim>(picEntry.action());
    alter(provisionalIdentityClaim,
          &ProvisionalIdentityClaim::authorSignatureByTankerKey);
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidSignature);
  }

  SUBCASE("should reject a ProvisionalIdentityClaim with an incorrect user ID")
  {
    authorUser.id[0]++;
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidUserId);
  }

  SUBCASE(
      "should reject a ProvisionalIdentityClaim with an incorrect user public "
      "key")
  {
    authorUser.userKey.value()[0]++;
    CHECK_VERIFICATION_FAILED_WITH(Verif::verifyProvisionalIdentityClaim(
                                       picEntry, authorUser, authorDevice),
                                   Error::VerificationCode::InvalidUserKey);
  }

  SUBCASE("should accept a valid ProvisionalIdentityClaim")
  {
    CHECK_NOTHROW(Verif::verifyProvisionalIdentityClaim(
        picEntry, authorUser, authorDevice));
  }
}
