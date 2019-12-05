#include <Tanker/Verif/DeviceRevocation.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Trustchain/Actions/DeviceCreation.hpp>
#include <Tanker/Trustchain/Actions/DeviceRevocation.hpp>
#include <Tanker/Trustchain/Actions/Nature.hpp>
#include <Tanker/Trustchain/ServerEntry.hpp>
#include <Tanker/Users/Device.hpp>
#include <Tanker/Users/User.hpp>
#include <Tanker/Verif/Errors/Errc.hpp>
#include <Tanker/Verif/Helpers.hpp>

#include <cassert>

using namespace Tanker::Trustchain::Actions;
using namespace Tanker::Trustchain;

namespace Tanker
{
namespace Verif
{
namespace
{
template <typename T>
bool has_duplicates(std::vector<T> vect)
{
  std::sort(vect.begin(), vect.end());
  return std::adjacent_find(vect.begin(), vect.end()) != vect.end();
}

void verifySubAction(DeviceRevocation1 const& deviceRevocation,
                     Users::Device const& target,
                     Users::User const& user)
{
  ensures(!user.userKey,
          Errc::InvalidUserKey,
          "A revocation V1 cannot be used on an user with a user key");
}

void verifySubAction(DeviceRevocation2 const& deviceRevocation,
                     Users::Device const& target,
                     Users::User const& user)
{
  if (!user.userKey)
  {
    ensures(deviceRevocation.previousPublicEncryptionKey().is_null(),
            Errc::InvalidEncryptionKey,
            "A revocation V2 should have an empty previousPublicEncryptionKey "
            "when its user has no userKey");

    ensures(deviceRevocation.sealedKeyForPreviousUserKey().is_null(),
            Errc::InvalidUserKey,
            "A revocation V2 should have an empty sealedKeyForPreviousUserKey "
            "when its user has no userKey");
  }
  else
  {
    ensures(deviceRevocation.previousPublicEncryptionKey() == *user.userKey,
            Errc::InvalidEncryptionKey,
            "A revocation V2 previousPublicEncryptionKey should be the same as "
            "its user userKey");
  }
  size_t const nbrDevicesNotRevoked = std::count_if(
      user.devices.begin(), user.devices.end(), [](auto const& device) {
        return device.revokedAtBlkIndex == std::nullopt;
      });
  ensures(deviceRevocation.sealedUserKeysForDevices().size() ==
              nbrDevicesNotRevoked - 1,
          Errc::InvalidUserKeys,
          "A revocation V2 should have exactly one userKey per remaining "
          "device of the user");
  for (auto userKey : deviceRevocation.sealedUserKeysForDevices())
  {
    ensures(userKey.first != target.id,
            Errc::InvalidUserKeys,
            "A revocation V2 should not have the target deviceId in the "
            "userKeys field");

    ensures(std::find_if(user.devices.begin(),
                         user.devices.end(),
                         [&](auto const& device) {
                           return userKey.first == device.id;
                         }) != user.devices.end(),
            Errc::InvalidUserKeys,
            "A revocation V2 should not have a key for another user's device");
  }

  ensures(!has_duplicates(deviceRevocation.sealedUserKeysForDevices()),
          Errc::InvalidUserKeys,
          "A revocation V2 should not have duplicates entries in the userKeys "
          "field");
}
}

void verifyDeviceRevocation(ServerEntry const& serverEntry,
                            Users::Device const& author,
                            Users::Device const& target,
                            Users::User const& user)
{
  assert(serverEntry.action().nature() == Nature::DeviceRevocation ||
         serverEntry.action().nature() == Nature::DeviceRevocation2);

  ensures(!author.revokedAtBlkIndex ||
              author.revokedAtBlkIndex > serverEntry.index(),
          Errc::InvalidAuthor,
          "Author device of revocation must not be revoked");

  ensures(!target.revokedAtBlkIndex,
          Errc::InvalidTargetDevice,
          "The target of a revocation must not be already revoked");

  ensures(std::find(user.devices.begin(), user.devices.end(), author) !=
                  user.devices.end() &&
              std::find(user.devices.begin(), user.devices.end(), target) !=
                  user.devices.end(),
          Errc::InvalidUser,
          "A device can only be revoked by another device of its user");

  ensures(
      Crypto::verify(serverEntry.hash(),
                     serverEntry.signature(),
                     author.publicSignatureKey),
      Errc::InvalidSignature,
      "device revocation block must be signed by the public signature key of "
      "its author");

  auto const& deviceRevocation = serverEntry.action().get<DeviceRevocation>();

  deviceRevocation.visit(
      [&](auto const& subAction) { verifySubAction(subAction, target, user); });
}
}
}
