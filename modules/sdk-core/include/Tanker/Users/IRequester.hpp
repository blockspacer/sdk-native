#pragma once

#include <Tanker/Crypto/SignatureKeyPair.hpp>
#include <Tanker/Trustchain/ResourceId.hpp>
#include <Tanker/Trustchain/ServerEntry.hpp>
#include <Tanker/Trustchain/TrustchainId.hpp>
#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/Types/Email.hpp>

#include <tconcurrent/coroutine.hpp>

#include <gsl-lite.hpp>

#include <tuple>
#include <vector>

namespace Tanker::Users
{
class IRequester
{
public:
  virtual ~IRequester() = default;
  virtual tc::cotask<std::vector<Trustchain::ServerEntry>> getMe() = 0;
  virtual tc::cotask<std::vector<Trustchain::ServerEntry>> getUsers(
      gsl::span<Trustchain::UserId const> userIds) = 0;
  virtual tc::cotask<std::vector<Trustchain::ServerEntry>> getUsers(
      gsl::span<Trustchain::DeviceId const> deviceIds) = 0;
  virtual tc::cotask<std::vector<std::string>> getKeyPublishes(
      gsl::span<Trustchain::ResourceId const> resourceIds) = 0;
  virtual tc::cotask<void> authenticate(
      Trustchain::TrustchainId const& trustchainId,
      Trustchain::UserId const& userId,
      Crypto::SignatureKeyPair const& userSignatureKeyPair) = 0;
  virtual tc::cotask<std::vector<
      std::tuple<Crypto::PublicSignatureKey, Crypto::PublicEncryptionKey>>>
  getPublicProvisionalIdentities(gsl::span<Email const>) = 0;
};
}
