#include <Tanker/Verif/UserGroupAddition.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Device.hpp>
#include <Tanker/Groups/Group.hpp>
#include <Tanker/Trustchain/Actions/UserGroupAddition.hpp>
#include <Tanker/UnverifiedEntry.hpp>
#include <Tanker/Verif/Helpers.hpp>

#include <cassert>

using namespace Tanker::Trustchain::Actions;

namespace Tanker
{
namespace Verif
{
void verifyUserGroupAddition(UnverifiedEntry const& entry,
                             Device const& author,
                             ExternalGroup const& group)
{
  assert(entry.nature == Nature::UserGroupAddition);

  ensures(!author.revokedAtBlkIndex || author.revokedAtBlkIndex > entry.index,
          Error::VerificationCode::InvalidAuthor,
          "A revoked device must not be the author of a UserGroupAddition");

  ensures(
      Crypto::verify(entry.hash, entry.signature, author.publicSignatureKey),
      Error::VerificationCode::InvalidSignature,
      "UserGroupAddition block must be signed by the author device");

  auto const& userGroupAddition = entry.action.get<UserGroupAddition>();

  ensures(userGroupAddition.previousGroupBlockHash() == group.lastBlockHash,
          Error::VerificationCode::InvalidGroup,
          "UserGroupAddition - previous group block does not match for this "
          "group id");

  ensures(Crypto::verify(userGroupAddition.signatureData(),
                         userGroupAddition.selfSignature(),
                         group.publicSignatureKey),
          Error::VerificationCode::InvalidSignature,
          "UserGroupAddition signature data must be signed with the group "
          "public key");
}
}
}
