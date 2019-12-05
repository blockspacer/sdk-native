#include <Tanker/Verif/DeviceCreation.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Trustchain/Actions/DeviceCreation.hpp>
#include <Tanker/Trustchain/Actions/Nature.hpp>
#include <Tanker/Trustchain/Actions/TrustchainCreation.hpp>
#include <Tanker/Users/Device.hpp>
#include <Tanker/Users/User.hpp>
#include <Tanker/Verif/Errors/Errc.hpp>
#include <Tanker/Verif/Helpers.hpp>

#include <cassert>

using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;

namespace Tanker
{
namespace Verif
{
namespace
{
bool verifySignature(DeviceCreation const& dc,
                     Crypto::PublicSignatureKey const& publicSignatureKey)
{
  auto const toVerify = dc.signatureData();
  return Crypto::verify(toVerify, dc.delegationSignature(), publicSignatureKey);
}

void verifySubAction(DeviceCreation::v1 const& deviceCreation,
                     Users::User const& user)
{
  ensures(!user.userKey.has_value(),
          Errc::InvalidUserKey,
          "A user must not have a user key to create a device creation v1");
}

void verifySubAction(DeviceCreation::v3 const& deviceCreation,
                     Users::User const& user)
{
  ensures(deviceCreation.publicUserEncryptionKey() == user.userKey,
          Errc::InvalidUserKey,
          "DeviceCreation v3 must have the last user key");
}
}

void verifyDeviceCreation(ServerEntry const& serverEntry,
                          Users::Device const& author,
                          Users::User const& user)
{
  auto const nature = serverEntry.action().nature();
  (void)nature;
  assert(nature == Nature::DeviceCreation || nature == Nature::DeviceCreation3);

  ensures(!author.revokedAtBlkIndex ||
              author.revokedAtBlkIndex > serverEntry.index(),
          Errc::InvalidAuthor,
          "author device must not be revoked");

  assert(std::find(user.devices.begin(), user.devices.end(), author) !=
         user.devices.end());

  auto const& deviceCreation = serverEntry.action().get<DeviceCreation>();

  ensures(Crypto::verify(serverEntry.hash(),
                         serverEntry.signature(),
                         deviceCreation.ephemeralPublicSignatureKey()),
          Errc::InvalidSignature,
          "device creation block must be signed by the ephemeral private "
          "signature key");
  ensures(verifySignature(deviceCreation, author.publicSignatureKey),
          Errc::InvalidDelegationSignature,
          "device creation's delegation signature must be signed by the "
          "author's private signature key");

  ensures(
      deviceCreation.userId() == user.id,
      Errc::InvalidUserId,
      "Device creation's user id must be the same than its parent device's");
  deviceCreation.visit(
      [&user](auto const& val) { verifySubAction(val, user); });
}

void verifyDeviceCreation(ServerEntry const& serverEntry,
                          TrustchainCreation const& author)
{
  assert(serverEntry.action().nature() == Nature::DeviceCreation ||
         serverEntry.action().nature() == Nature::DeviceCreation3);

  auto const& deviceCreation = serverEntry.action().get<DeviceCreation>();

  ensures(Crypto::verify(serverEntry.hash(),
                         serverEntry.signature(),
                         deviceCreation.ephemeralPublicSignatureKey()),
          Errc::InvalidSignature,
          "device creation block must be signed by the ephemeral private "
          "signature key");
  ensures(verifySignature(deviceCreation, author.publicSignatureKey()),
          Errc::InvalidDelegationSignature,
          "device creation's delegation signature must be signed by the "
          "author's private signature key");
}
}
}
