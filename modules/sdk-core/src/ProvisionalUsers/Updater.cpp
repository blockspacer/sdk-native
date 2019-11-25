#include <Tanker/ProvisionalUsers/Updater.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Crypto/Format/Format.hpp>
#include <Tanker/Errors/AssertionError.hpp>
#include <Tanker/Errors/Errc.hpp>
#include <Tanker/Errors/Exception.hpp>
#include <Tanker/Log/Log.hpp>
#include <Tanker/Trustchain/Actions/ProvisionalIdentityClaim.hpp>
#include <Tanker/Verif/Errors/Errc.hpp>
#include <Tanker/Verif/Errors/ErrcCategory.hpp>
#include <Tanker/Verif/Helpers.hpp>
#include <Tanker/Verif/ProvisionalIdentityClaim.hpp>

#include <boost/container/flat_map.hpp>

TLOG_CATEGORY("ProvisionalUsersUpdater");

using Tanker::Trustchain::Actions::ProvisionalIdentityClaim;
using namespace Tanker::Errors;

namespace Tanker
{
namespace ProvisionalUsers
{
namespace Updater
{
namespace
{
using DeviceMap = boost::container::flat_map<Trustchain::DeviceId, Device>;

tc::cotask<DeviceMap> extractAuthors(
    ContactStore const& contactStore,
    std::vector<Trustchain::ServerEntry> const& entries)
{
  DeviceMap out;

  for (auto const& entry : entries)
  {
    if (out.find(Trustchain::DeviceId{entry.author()}) != out.end())
      continue;

    auto const author =
        TC_AWAIT(contactStore.findDevice(Trustchain::DeviceId{entry.author()}));
    if (author)
      out[Trustchain::DeviceId{entry.author()}] = *author;
    else
      // we should have all the devices because they are *our* devices
      throw Errors::formatEx(Errors::Errc::InternalError,
                             "missing device for claim verification: {}",
                             entry.author());
  }

  TC_RETURN(out);
}

tc::cotask<SecretProvisionalUser> extractKeysToStore(
    UserKeyStore const& userKeyStore, Entry const& entry)
{
  auto const& provisionalIdentityClaim =
      entry.action.get<ProvisionalIdentityClaim>();

  auto const userKeyPair = TC_AWAIT(userKeyStore.findKeyPair(
      provisionalIdentityClaim.userPublicEncryptionKey()));

  if (!userKeyPair)
  {
    throw Exception(make_error_code(Errc::InternalError),
                    "cannot find user key for claim decryption");
  }

  auto const provisionalIdentityKeys = Crypto::sealDecrypt(
      provisionalIdentityClaim.sealedPrivateEncryptionKeys(), *userKeyPair);

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

  TC_RETURN((
      SecretProvisionalUser{provisionalIdentityClaim.appSignaturePublicKey(),
                            provisionalIdentityClaim.tankerSignaturePublicKey(),
                            appEncryptionKeyPair,
                            tankerEncryptionKeyPair}));
}
}

tc::cotask<void> applyEntry(UserKeyStore& userKeyStore,
                            ProvisionalUserKeysStore& provisionalUserKeysStore,
                            Entry const& entry)
{
  auto const toStore = TC_AWAIT(extractKeysToStore(userKeyStore, entry));

  TC_AWAIT(provisionalUserKeysStore.putProvisionalUserKeys(
      toStore.appSignaturePublicKey,
      toStore.tankerSignaturePublicKey,
      {toStore.appEncryptionKeyPair, toStore.tankerEncryptionKeyPair}));
}

tc::cotask<std::vector<SecretProvisionalUser>> processClaimEntries(
    ContactStore const& contactStore,
    UserKeyStore const& userKeyStore,
    std::vector<Trustchain::ServerEntry> const& serverEntries)
{
  auto const authors = TC_AWAIT(extractAuthors(contactStore, serverEntries));

  std::vector<SecretProvisionalUser> out;
  for (auto const& serverEntry : serverEntries)
  {
    try
    {
      auto const authorIt =
          authors.find(Trustchain::DeviceId{serverEntry.author()});
      Verif::ensures(authorIt != authors.end(),
                     Verif::Errc::InvalidAuthor,
                     "author not found");
      auto const& author = authorIt->second;
      if (!serverEntry.action().holds_alternative<ProvisionalIdentityClaim>())
        throw Errors::AssertionError(fmt::format(
            "cannot handle nature: {}", serverEntry.action().nature()));

      Verif::verifyProvisionalIdentityClaim(serverEntry, author);
      auto const entry = Verif::makeVerifiedEntry(serverEntry);

      out.push_back(TC_AWAIT(extractKeysToStore(userKeyStore, entry)));
    }
    catch (Errors::Exception const& err)
    {
      if (err.errorCode().category() == Verif::ErrcCategory())
      {
        TERROR("skipping invalid claim block {}: {}",
               serverEntry.hash(),
               err.what());
      }
      else
        throw;
    }
  }
  TC_RETURN(out);
}
}
}
}
