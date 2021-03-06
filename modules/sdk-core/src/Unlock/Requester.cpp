#include <Tanker/Client.hpp>
#include <Tanker/Unlock/Requester.hpp>

#include <nlohmann/json.hpp>

namespace Tanker::Unlock
{
static void from_json(nlohmann::json const& j, UserStatusResult& result)
{
  j.at("device_exists").get_to(result.deviceExists);
  result.userExists = j.at("user_exists").get<bool>();
  auto const lastReset = j.at("last_reset").get<std::string>();
  if (!lastReset.empty())
    result.lastReset =
        cppcodec::base64_rfc4648::decode<Crypto::Hash>(lastReset);
  else
    result.lastReset = Crypto::Hash{};
}

Requester::Requester(Client* client) : _client(client)
{
}

tc::cotask<UserStatusResult> Requester::userStatus(
    Trustchain::TrustchainId const& trustchainId,
    Trustchain::UserId const& userId,
    Crypto::PublicSignatureKey const& publicSignatureKey)
{
  nlohmann::json request{
      {"trustchain_id", trustchainId},
      {"user_id", userId},
      {"device_public_signature_key", publicSignatureKey},
  };

  auto const reply = TC_AWAIT(_client->emit("get user status", request));

  TC_RETURN(reply.get<UserStatusResult>());
}

tc::cotask<void> Requester::setVerificationMethod(
    Trustchain::TrustchainId const& trustchainId,
    Trustchain::UserId const& userId,
    Unlock::Request const& request)
{
  nlohmann::json payload{
      {"trustchain_id", trustchainId},
      {"user_id", userId},
      {"verification", request},
  };
  TC_AWAIT(_client->emit("set verification method", payload));
}

tc::cotask<std::vector<std::uint8_t>> Requester::fetchVerificationKey(
    Trustchain::TrustchainId const& trustchainId,
    Trustchain::UserId const& userId,
    Unlock::Request const& request)
{
  nlohmann::json payload{
      {"trustchain_id", trustchainId},
      {"user_id", userId},
      {"verification", request},
  };
  auto const response =
      TC_AWAIT(_client->emit("get verification key", payload));
  TC_RETURN(cppcodec::base64_rfc4648::decode<std::vector<uint8_t>>(
      response.at("encrypted_verification_key").get<std::string>()));
}

tc::cotask<std::vector<Unlock::VerificationMethod>>
Requester::fetchVerificationMethods(
    Trustchain::TrustchainId const& trustchainId,
    Trustchain::UserId const& userId)
{
  auto const request =
      nlohmann::json{{"trustchain_id", trustchainId}, {"user_id", userId}};

  auto const reply =
      TC_AWAIT(_client->emit("get verification methods", request));
  auto methods = reply.at("verification_methods")
                     .get<std::vector<Unlock::VerificationMethod>>();
  TC_RETURN(methods);
}

tc::cotask<void> Requester::createUser(
    Trustchain::TrustchainId const& trustchainId,
    Trustchain::UserId const& userId,
    gsl::span<uint8_t const> userCreation,
    gsl::span<uint8_t const> firstDevice,
    Unlock::Request const& verificationRequest,
    gsl::span<uint8_t const> encryptedVerificationKey)
{
  nlohmann::json request{
      {"trustchain_id", trustchainId},
      {"user_id", userId},
      {"user_creation_block", cppcodec::base64_rfc4648::encode(userCreation)},
      {"first_device_block", cppcodec::base64_rfc4648::encode(firstDevice)},
      {"encrypted_unlock_key",
       cppcodec::base64_rfc4648::encode(encryptedVerificationKey)},
      {"verification", verificationRequest},
  };
  auto const reply = TC_AWAIT(_client->emit("create user 2", request));
}
}
