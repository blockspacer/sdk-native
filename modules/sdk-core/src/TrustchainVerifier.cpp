#include <Tanker/TrustchainVerifier.hpp>

#include <Tanker/ContactStore.hpp>
#include <Tanker/DataStore/ADatabase.hpp>
#include <Tanker/Device.hpp>
#include <Tanker/Entry.hpp>
#include <Tanker/Error.hpp>
#include <Tanker/Groups/Group.hpp>
#include <Tanker/Groups/GroupStore.hpp>
#include <Tanker/Trustchain/Actions/Nature.hpp>
#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/User.hpp>
#include <Tanker/Verif/DeviceCreation.hpp>
#include <Tanker/Verif/DeviceRevocation.hpp>
#include <Tanker/Verif/Helpers.hpp>
#include <Tanker/Verif/KeyPublishToDevice.hpp>
#include <Tanker/Verif/KeyPublishToUser.hpp>
#include <Tanker/Verif/KeyPublishToUserGroup.hpp>
#include <Tanker/Verif/ProvisionalIdentityClaim.hpp>
#include <Tanker/Verif/TrustchainCreation.hpp>
#include <Tanker/Verif/UserGroupAddition.hpp>
#include <Tanker/Verif/UserGroupCreation.hpp>

#include <cassert>

using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;

namespace Tanker
{
namespace
{
Entry toEntry(Trustchain::ServerEntry const& se)
{
  return {
      se.index(), se.action().nature(), se.author(), se.action(), se.hash()};
}
}

TrustchainVerifier::TrustchainVerifier(Trustchain::TrustchainId const& id,
                                       DataStore::ADatabase* db,
                                       ContactStore* contacts,
                                       GroupStore* groups)
  : _trustchainId(id), _db(db), _contacts(contacts), _groups(groups)
{
}

tc::cotask<Entry> TrustchainVerifier::verify(
    Trustchain::ServerEntry const& e) const
{
  switch (e.action().nature())
  {
  case Nature::TrustchainCreation:
    Verif::verifyTrustchainCreation(e, _trustchainId);
    TC_RETURN(toEntry(e));
  case Nature::DeviceCreation:
  case Nature::DeviceCreation2:
  case Nature::DeviceCreation3:
    TC_RETURN(TC_AWAIT(handleDeviceCreation(e)));
  case Nature::KeyPublishToDevice:
  case Nature::KeyPublishToUser:
  case Nature::KeyPublishToProvisionalUser:
    TC_RETURN(TC_AWAIT(handleKeyPublish(e)));
  case Nature::KeyPublishToUserGroup:
    TC_RETURN(TC_AWAIT(handleKeyPublishToUserGroups(e)));
  case Nature::DeviceRevocation:
  case Nature::DeviceRevocation2:
    TC_RETURN(TC_AWAIT(handleDeviceRevocation(e)));
  case Nature::UserGroupAddition:
  case Nature::UserGroupAddition2:
    TC_RETURN(TC_AWAIT(handleUserGroupAddition(e)));
  case Nature::UserGroupCreation:
  case Nature::UserGroupCreation2:
    TC_RETURN(TC_AWAIT(handleUserGroupCreation(e)));
  case Nature::ProvisionalIdentityClaim:
    TC_RETURN(TC_AWAIT(handleProvisionalIdentityClaim(e)));
  }
  throw std::runtime_error(
      "Assertion failed: Invalid nature for unverified entry");
}

tc::cotask<Entry> TrustchainVerifier::handleDeviceCreation(
    Trustchain::ServerEntry const& dc) const
{
  if (dc.author().base() == _trustchainId.base())
  {
    auto const trustchainCreation = TrustchainCreation{
        TC_AWAIT(_db->findTrustchainPublicSignatureKey()).value()};
    Verif::verifyDeviceCreation(dc, trustchainCreation);
  }
  else
  {
    auto const user =
        TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(dc.author())));
    auto const authorDevice = getDevice(user, dc.author());
    Verif::verifyDeviceCreation(dc, authorDevice, user);
  }
  TC_RETURN(toEntry(dc));
}

tc::cotask<Entry> TrustchainVerifier::handleKeyPublish(
    Trustchain::ServerEntry const& kp) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(kp.author())));

  auto const authorDevice = getDevice(user, kp.author());
  auto const nature = kp.action().nature();

  if (nature == Nature::KeyPublishToDevice)
  {
    Verif::verifyKeyPublishToDevice(kp, authorDevice, user);
  }
  else if (nature == Nature::KeyPublishToUser ||
           nature == Nature::KeyPublishToProvisionalUser)
  {
    Verif::verifyKeyPublishToUser(kp, authorDevice);
  }
  else
  {
    assert(false &&
           "nature must be "
           "KeyPublishToDevice/KeyPublishToUser/KeyPublishToProvisionalUser");
  }
  TC_RETURN(toEntry(kp));
}

tc::cotask<Entry> TrustchainVerifier::handleKeyPublishToUserGroups(
    Trustchain::ServerEntry const& kp) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(kp.author())));
  auto const& keyPublishToUserGroup =
      kp.action().get<KeyPublish>().get<KeyPublish::ToUserGroup>();
  auto const group = TC_AWAIT(getGroupByEncryptionKey(
      keyPublishToUserGroup.recipientPublicEncryptionKey()));
  auto const authorDevice = getDevice(user, kp.author());
  Verif::verifyKeyPublishToUserGroup(kp, authorDevice, group);

  TC_RETURN(toEntry(kp));
}

tc::cotask<Entry> TrustchainVerifier::handleDeviceRevocation(
    Trustchain::ServerEntry const& dr) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(dr.author())));

  auto const& revocation = dr.action().get<DeviceRevocation>();
  auto const authorDevice = getDevice(user, dr.author());
  auto const targetDevice =
      getDevice(user, static_cast<Crypto::Hash>(revocation.deviceId()));
  Verif::verifyDeviceRevocation(dr, authorDevice, targetDevice, user);

  TC_RETURN(toEntry(dr));
}

tc::cotask<Entry> TrustchainVerifier::handleUserGroupAddition(
    Trustchain::ServerEntry const& ga) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(ga.author())));
  auto const& userGroupAddition = ga.action().get<UserGroupAddition>();
  auto const group = TC_AWAIT(getGroupById(userGroupAddition.groupId()));
  auto const authorDevice = getDevice(user, ga.author());
  Verif::verifyUserGroupAddition(ga, authorDevice, group);

  TC_RETURN(toEntry(ga));
}

tc::cotask<Entry> TrustchainVerifier::handleUserGroupCreation(
    Trustchain::ServerEntry const& gc) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(gc.author())));
  auto const& userGroupCreation = gc.action().get<UserGroupCreation>();
  auto const authorDevice = getDevice(user, gc.author());

  auto const group = TC_AWAIT(_groups->findExternalByPublicEncryptionKey(
      userGroupCreation.publicEncryptionKey()));
  Verif::ensures(!group,
                 Error::VerificationCode::InvalidGroup,
                 "UserGroupCreation - group already exist");

  Verif::verifyUserGroupCreation(gc, authorDevice);

  TC_RETURN(toEntry(gc));
}

tc::cotask<Entry> TrustchainVerifier::handleProvisionalIdentityClaim(
    Trustchain::ServerEntry const& claim) const
{
  auto const user =
      TC_AWAIT(getUserByDeviceId(static_cast<DeviceId>(claim.author())));
  auto const authorDevice = getDevice(user, claim.author());
  Verif::verifyProvisionalIdentityClaim(claim, user, authorDevice);

  TC_RETURN(toEntry(claim));
}

tc::cotask<User> TrustchainVerifier::getUser(UserId const& userId) const
{
  auto const user = TC_AWAIT(_contacts->findUser(userId));
  Verif::ensures(
      user.has_value(), Error::VerificationCode::InvalidUser, "user not found");
  TC_RETURN(*user);
}

tc::cotask<User> TrustchainVerifier::getUserByDeviceId(
    Trustchain::DeviceId const& deviceId) const
{
  auto const optUserId = TC_AWAIT(_contacts->findUserIdByDeviceId(deviceId));

  Verif::ensures(optUserId.has_value(),
                 Error::VerificationCode::InvalidAuthor,
                 "user id not found");

  return getUser(*optUserId);
}

Device TrustchainVerifier::getDevice(User const& user,
                                     Crypto::Hash const& deviceHash) const
{
  auto const device = std::find_if(
      user.devices.begin(), user.devices.end(), [&](auto const& device) {
        return device.id.base() == deviceHash.base();
      });
  assert(device != user.devices.end() && "device should belong to user");
  return *device;
}

tc::cotask<ExternalGroup> TrustchainVerifier::getGroupByEncryptionKey(
    Crypto::PublicEncryptionKey const& recipientPublicEncryptionKey) const
{
  auto const group = TC_AWAIT(
      _groups->findExternalByPublicEncryptionKey(recipientPublicEncryptionKey));
  Verif::ensures(group.has_value(),
                 Error::VerificationCode::InvalidGroup,
                 "group not found");
  TC_RETURN(*group);
}

tc::cotask<ExternalGroup> TrustchainVerifier::getGroupById(
    Trustchain::GroupId const& groupId) const
{
  auto const group = TC_AWAIT(_groups->findExternalById(groupId));
  Verif::ensures(group.has_value(),
                 Error::VerificationCode::InvalidGroup,
                 "group not found");
  TC_RETURN(*group);
}
}
