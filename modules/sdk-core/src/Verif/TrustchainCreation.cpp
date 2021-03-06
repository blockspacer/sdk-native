#include <Tanker/Verif/TrustchainCreation.hpp>

#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/Verif/Errors/Errc.hpp>
#include <Tanker/Verif/Helpers.hpp>

#include <cassert>

using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;

namespace Tanker
{
namespace Verif
{
Entry verifyTrustchainCreation(ServerEntry const& rootEntry,
                               TrustchainId const& currentTrustchainId)
{
  assert(rootEntry.action().nature() == Nature::TrustchainCreation);

  ensures(rootEntry.hash().base() == currentTrustchainId.base(),
          Errc::InvalidHash,
          "root block hash must be the trustchain id");
  ensures(rootEntry.author().is_null(),
          Errc::InvalidAuthor,
          "author must be zero-filled");
  ensures(rootEntry.signature().is_null(),
          Errc::InvalidSignature,
          "signature must be zero-filled");
  return makeVerifiedEntry(rootEntry);
}
}
}
