#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/Trustchain/UserId.hpp>

namespace Tanker
{
namespace Crypto
{
template class BasicHash<struct Trustchain::detail::UserIdImpl>;
template class BasicHash<struct Trustchain::detail::TrustchainIdImpl>;
}
}