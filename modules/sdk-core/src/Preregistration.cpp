#include <Tanker/Preregistration.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Error.hpp>

namespace Tanker
{
namespace Preregistration
{
tc::cotask<void> applyEntry(UserKeyStore& userKeyStore,
                            ProvisionalUserKeysStore& provisionalUserKeysStore,
                            Entry const& entry)
{
  auto const& provisionalIdentityClaim =
      mpark::get<ProvisionalIdentityClaim>(entry.action.variant());

  auto const userKeyPair = TC_AWAIT(userKeyStore.findKeyPair(
      provisionalIdentityClaim.userPublicEncryptionKey));

  if (!userKeyPair)
    throw Error::UserKeyNotFound("can't find user key for claim decryption");

  auto const provisionalIdentityKeys = Crypto::sealDecrypt(
      provisionalIdentityClaim.encryptedPrivateKeys, *userKeyPair);

  // this size is ensured because the encrypted buffer has a fixed size
  assert(provisionalIdentityKeys.size() ==
         2 * Crypto::PrivateEncryptionKey::arraySize);

  auto const appEncryptionKeyPair =
      Crypto::makeEncryptionKeyPair(Crypto::PrivateEncryptionKey(
          gsl::make_span(provisionalIdentityKeys)
              .subspan(0, Crypto::PrivateEncryptionKey::arraySize)));
  auto const tankerEncryptionKeyPair =
      Crypto::makeEncryptionKeyPair(Crypto::PrivateEncryptionKey(
          gsl::make_span(provisionalIdentityKeys)
              .subspan(Crypto::PrivateEncryptionKey::arraySize)));

  TC_AWAIT(provisionalUserKeysStore.putProvisionalUserKeys(
      provisionalIdentityClaim.appSignaturePublicKey,
      provisionalIdentityClaim.tankerSignaturePublicKey,
      {appEncryptionKeyPair, tankerEncryptionKeyPair}));
}
}
}