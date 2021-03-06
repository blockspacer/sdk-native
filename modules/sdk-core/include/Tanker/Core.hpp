#pragma once

#include <Tanker/AttachResult.hpp>
#include <Tanker/EncryptionSession.hpp>
#include <Tanker/Network/SdkInfo.hpp>
#include <Tanker/ResourceKeys/Store.hpp>
#include <Tanker/Streams/DecryptionStreamAdapter.hpp>
#include <Tanker/Streams/EncryptionStream.hpp>
#include <Tanker/Streams/InputSource.hpp>
#include <Tanker/Trustchain/DeviceId.hpp>
#include <Tanker/Types/SGroupId.hpp>
#include <Tanker/Types/SPublicIdentity.hpp>
#include <Tanker/Types/SResourceId.hpp>
#include <Tanker/Types/SSecretProvisionalIdentity.hpp>
#include <Tanker/Types/VerificationKey.hpp>
#include <Tanker/Unlock/Verification.hpp>
#include <Tanker/Users/Device.hpp>

#include <gsl-lite.hpp>
#include <tconcurrent/coroutine.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Tanker
{
class Session;

class Core
{
public:
  using SessionClosedHandler = std::function<void()>;

  ~Core();
  Core(std::string url, Network::SdkInfo info, std::string writablePath);

  tc::cotask<Status> start(std::string const& identity);
  tc::cotask<void> registerIdentity(Unlock::Verification const& verification);
  tc::cotask<void> verifyIdentity(Unlock::Verification const& verification);

  tc::cotask<void> encrypt(
      uint8_t* encryptedData,
      gsl::span<uint8_t const> clearData,
      std::vector<SPublicIdentity> const& spublicIdentities = {},
      std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<std::vector<uint8_t>> encrypt(
      gsl::span<uint8_t const> clearData,
      std::vector<SPublicIdentity> const& spublicIdentities = {},
      std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<void> decrypt(uint8_t* decryptedData,
                           gsl::span<uint8_t const> encryptedData);

  tc::cotask<std::vector<uint8_t>> decrypt(
      gsl::span<uint8_t const> encryptedData);

  tc::cotask<void> share(std::vector<SResourceId> const& sresourceIds,
                         std::vector<SPublicIdentity> const& publicIdentities,
                         std::vector<SGroupId> const& groupIds);

  tc::cotask<SGroupId> createGroup(
      std::vector<SPublicIdentity> const& spublicIdentities);
  tc::cotask<void> updateGroupMembers(
      SGroupId const& groupIdString,
      std::vector<SPublicIdentity> const& spublicIdentitiesToAdd);

  tc::cotask<void> setVerificationMethod(Unlock::Verification const& method);
  tc::cotask<std::vector<Unlock::VerificationMethod>> getVerificationMethods();
  tc::cotask<VerificationKey> generateVerificationKey() const;

  tc::cotask<AttachResult> attachProvisionalIdentity(
      SSecretProvisionalIdentity const& sidentity);
  tc::cotask<void> verifyProvisionalIdentity(
      Unlock::Verification const& verification);

  tc::cotask<void> revokeDevice(Trustchain::DeviceId const& deviceId);

  Trustchain::DeviceId const& deviceId() const;
  tc::cotask<std::vector<Users::Device>> getDeviceList() const;

  tc::cotask<Streams::EncryptionStream> makeEncryptionStream(
      Streams::InputSource,
      std::vector<SPublicIdentity> const& suserIds = {},
      std::vector<SGroupId> const& sgroupIds = {});

  tc::cotask<Streams::DecryptionStreamAdapter> makeDecryptionStream(
      Streams::InputSource);

  tc::cotask<EncryptionSession> makeEncryptionSession(
      std::vector<SPublicIdentity> const& spublicIdentities,
      std::vector<SGroupId> const& sgroupIds);

  Status status() const;

  static Trustchain::ResourceId getResourceId(
      gsl::span<uint8_t const> encryptedData);

  void stop();
  tc::cotask<void> nukeDatabase();
  void setSessionClosedHandler(SessionClosedHandler);

private:
  tc::cotask<Status> startImpl(std::string const& b64Identity);

  tc::cotask<VerificationKey> fetchVerificationKey(
      Unlock::Verification const& verification);
  tc::cotask<VerificationKey> getVerificationKey(Unlock::Verification const&);
  tc::cotask<Crypto::SymmetricKey> getResourceKey(
      Trustchain::ResourceId const&);

  void assertStatus(Status wanted, std::string const& string) const;
  void reset();
  template <typename F>
  decltype(std::declval<F>()()) resetOnFailure(F&& f);

private:
  std::string _url;
  Network::SdkInfo _info;
  std::string _writablePath;
  SessionClosedHandler _sessionClosed;
  std::shared_ptr<Session> _session;
};
}
