#include <Tanker/AsyncCore.hpp>

#include <Tanker/Encryptor.hpp>
#include <Tanker/Log/LogHandler.hpp>
#include <Tanker/Status.hpp>
#include <Tanker/Trustchain/DeviceId.hpp>
#include <Tanker/Types/VerificationKey.hpp>
#include <Tanker/Utils.hpp>
#include <Tanker/Version.hpp>

#include <cppcodec/base64_rfc4648.hpp>
#include <tconcurrent/async.hpp>
#include <tconcurrent/coroutine.hpp>
#include <tconcurrent/thread_pool.hpp>

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace Tanker
{
namespace
{
auto makeEventHandler(task_canceler& tc, std::function<void()> cb)
{
  return [&tc, cb = std::move(cb)] {
    tc.run([&cb] { return tc::async([cb = std::move(cb)] { cb(); }); });
  };
}
}

AsyncCore::AsyncCore(std::string url,
                     Network::SdkInfo info,
                     std::string writablePath)
  : _core(std::move(url), std::move(info), std::move(writablePath))
{
}

AsyncCore::~AsyncCore() = default;

tc::future<void> AsyncCore::destroy()
{
  if (tc::get_default_executor().is_in_this_context())
    return tc::sync([this] { delete this; });
  else
    return tc::async([this] { delete this; });
}

tc::shared_future<Status> AsyncCore::start(std::string const& identity)
{
  return runResumable([=]() -> tc::cotask<Status> {
    TC_RETURN(TC_AWAIT(this->_core.start(identity)));
  });
}

tc::shared_future<void> AsyncCore::registerIdentity(
    Unlock::Verification const& verification)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.registerIdentity(verification));
  });
}

tc::shared_future<void> AsyncCore::verifyIdentity(
    Unlock::Verification const& verification)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.verifyIdentity(verification));
  });
}

tc::shared_future<void> AsyncCore::stop()
{
  return _taskCanceler.run(
      [&] { return tc::async([this] { this->_core.stop(); }); });
}

Tanker::Status AsyncCore::status() const
{
  return this->_core.status();
}

tc::shared_future<void> AsyncCore::encrypt(
    uint8_t* encryptedData,
    gsl::span<uint8_t const> clearData,
    std::vector<SPublicIdentity> const& publicIdentities,
    std::vector<SGroupId> const& groupIds)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.encrypt(
        encryptedData, clearData, publicIdentities, groupIds));
  });
}

tc::shared_future<void> AsyncCore::decrypt(
    uint8_t* decryptedData, gsl::span<uint8_t const> encryptedData)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.decrypt(decryptedData, encryptedData));
  });
}

tc::shared_future<std::vector<uint8_t>> AsyncCore::encrypt(
    gsl::span<uint8_t const> clearData,
    std::vector<SPublicIdentity> const& publicIdentities,
    std::vector<SGroupId> const& groupIds)
{
  return runResumable([=]() -> tc::cotask<std::vector<uint8_t>> {
    TC_RETURN(TC_AWAIT(_core.encrypt(clearData, publicIdentities, groupIds)));
  });
}

tc::shared_future<std::vector<uint8_t>> AsyncCore::decrypt(
    gsl::span<uint8_t const> encryptedData)
{
  return runResumable([=]() -> tc::cotask<std::vector<uint8_t>> {
    std::vector<uint8_t> decryptedData(Encryptor::decryptedSize(encryptedData));
    TC_AWAIT(_core.decrypt(decryptedData.data(), encryptedData));
    TC_RETURN(std::move(decryptedData));
  });
}

tc::shared_future<void> AsyncCore::share(
    std::vector<SResourceId> const& resourceId,
    std::vector<SPublicIdentity> const& publicIdentities,
    std::vector<SGroupId> const& groupIds)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.share(resourceId, publicIdentities, groupIds));
  });
}

tc::shared_future<SGroupId> AsyncCore::createGroup(
    std::vector<SPublicIdentity> const& members)
{
  return runResumable([=]() -> tc::cotask<SGroupId> {
    TC_RETURN(TC_AWAIT(this->_core.createGroup(members)));
  });
}

tc::shared_future<void> AsyncCore::updateGroupMembers(
    SGroupId const& groupId, std::vector<SPublicIdentity> const& usersToAdd)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.updateGroupMembers(groupId, usersToAdd));
  });
}

tc::shared_future<VerificationKey> AsyncCore::generateVerificationKey()
{
  return runResumable([this]() -> tc::cotask<VerificationKey> {
    TC_RETURN(TC_AWAIT(this->_core.generateVerificationKey()));
  });
}

tc::shared_future<void> AsyncCore::setVerificationMethod(
    Unlock::Verification const& method)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.setVerificationMethod(method));
  });
}

tc::shared_future<std::vector<Unlock::VerificationMethod>>
AsyncCore::getVerificationMethods()
{
  return runResumable(
      [=]() -> tc::cotask<std::vector<Unlock::VerificationMethod>> {
        TC_RETURN(TC_AWAIT(this->_core.getVerificationMethods()));
      });
}

tc::shared_future<AttachResult> AsyncCore::attachProvisionalIdentity(
    SSecretProvisionalIdentity const& sidentity)
{
  return runResumable([=]() -> tc::cotask<AttachResult> {
    TC_RETURN(TC_AWAIT(this->_core.attachProvisionalIdentity(sidentity)));
  });
}

tc::shared_future<void> AsyncCore::verifyProvisionalIdentity(
    Unlock::Verification const& verification)
{
  return runResumable([=]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.verifyProvisionalIdentity(verification));
  });
}

tc::shared_future<SDeviceId> AsyncCore::deviceId() const
{
  return _taskCanceler.run([&] {
    return tc::async([this] {
      return SDeviceId(cppcodec::base64_rfc4648::encode(_core.deviceId()));
    });
  });
}

tc::shared_future<std::vector<Users::Device>> AsyncCore::getDeviceList()
{
  return runResumable([this]() -> tc::cotask<std::vector<Users::Device>> {
    auto devices = TC_AWAIT(this->_core.getDeviceList());
    devices.erase(
        std::remove_if(devices.begin(), devices.end(), [](auto const& device) {
          return device.isGhostDevice();
        }));
    TC_RETURN(devices);
  });
}

tc::shared_future<void> AsyncCore::revokeDevice(SDeviceId const& deviceId)
{
  return runResumable([this, deviceId]() -> tc::cotask<void> {
    TC_AWAIT(this->_core.revokeDevice(
        base64DecodeArgument<Trustchain::DeviceId>(deviceId.string())));
  });
}

void AsyncCore::connectSessionClosed(std::function<void()> cb)
{
  this->_core.setSessionClosedHandler(
      makeEventHandler(this->_taskCanceler, std::move(cb)));
}

void AsyncCore::disconnectSessionClosed()
{
  this->_core.setSessionClosedHandler(nullptr);
}

void AsyncCore::connectDeviceRevoked(std::function<void()> cb)
{
  this->_asyncDeviceRevoked =
      makeEventHandler(this->_taskCanceler, std::move(cb));
}

void AsyncCore::disconnectDeviceRevoked()
{
  this->_asyncDeviceRevoked = nullptr;
}

tc::thread_pool& AsyncCore::getLogHandlerThreadPool()
{
  // Android's libart doesn't like it when we call java from a coroutine, so we
  // make a thread pool here.
  static tc::thread_pool tp;
  if (!tp.is_running())
    tp.start(1);
  return tp;
}

void AsyncCore::setLogHandler(Log::LogHandler handler)
{
  auto& tp = getLogHandlerThreadPool();
  Log::setLogHandler([handler, &tp](Log::Record const& record) {
    tc::async(tp, [=] { handler(record); }).get();
  });
}

uint64_t AsyncCore::encryptedSize(uint64_t clearSize)
{
  return Encryptor::encryptedSize(clearSize);
}

expected<uint64_t> AsyncCore::decryptedSize(
    gsl::span<uint8_t const> encryptedData)
{
  return tc::sync([&] { return Encryptor::decryptedSize(encryptedData); });
}

expected<SResourceId> AsyncCore::getResourceId(
    gsl::span<uint8_t const> encryptedData)
{
  return tc::sync([&] {
    return cppcodec::base64_rfc4648::encode<SResourceId>(
        Core::getResourceId(encryptedData));
  });
}

tc::shared_future<Streams::EncryptionStream> AsyncCore::makeEncryptionStream(
    Streams::InputSource cb,
    std::vector<SPublicIdentity> const& suserIds,
    std::vector<SGroupId> const& sgroupIds)
{
  // mutable so that we can move cb (otherwise it will be a const&&)
  return _taskCanceler.run([&]() mutable {
    return tc::async_resumable(
        [=, cb = std::move(cb)]() -> tc::cotask<Streams::EncryptionStream> {
          TC_RETURN(TC_AWAIT(this->_core.makeEncryptionStream(
              std::move(cb), suserIds, sgroupIds)));
        });
  });
}

tc::shared_future<Streams::DecryptionStreamAdapter>
AsyncCore::makeDecryptionStream(Streams::InputSource cb)
{
  return _taskCanceler.run([&] {
    return tc::async_resumable(
        [this,
         cb = std::move(cb)]() -> tc::cotask<Streams::DecryptionStreamAdapter> {
          TC_RETURN(TC_AWAIT(this->_core.makeDecryptionStream(std::move(cb))));
        });
  });
}

tc::shared_future<EncryptionSession> AsyncCore::makeEncryptionSession(
    std::vector<SPublicIdentity> const& publicIdentities,
    std::vector<SGroupId> const& groupIds)
{
  return runResumable([=]() -> tc::cotask<EncryptionSession> {
    TC_RETURN(TC_AWAIT(
        this->_core.makeEncryptionSession(publicIdentities, groupIds)));
  });
}

std::string const& AsyncCore::version()
{
  return TANKER_VERSION;
}
}
