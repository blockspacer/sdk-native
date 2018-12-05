#include <Tanker/Session.hpp>

#include <Tanker/Actions/DeviceCreation.hpp>
#include <Tanker/Actions/KeyPublishToUser.hpp>
#include <Tanker/Actions/KeyPublishToUserGroup.hpp>
#include <Tanker/Actions/UserKeyPair.hpp>
#include <Tanker/BlockGenerator.hpp>
#include <Tanker/ChunkEncryptor.hpp>
#include <Tanker/Client.hpp>
#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Crypto/KeyFormat.hpp>
#include <Tanker/DeviceKeyStore.hpp>
#include <Tanker/Encryptor.hpp>
#include <Tanker/Entry.hpp>
#include <Tanker/Error.hpp>
#include <Tanker/Groups/GroupUpdater.hpp>
#include <Tanker/Groups/Manager.hpp>
#include <Tanker/Log.hpp>
#include <Tanker/ReceiveKey.hpp>
#include <Tanker/RecipientNotFound.hpp>
#include <Tanker/ResourceKeyStore.hpp>
#include <Tanker/Share.hpp>
#include <Tanker/Trustchain.hpp>
#include <Tanker/TrustchainPuller.hpp>
#include <Tanker/Types/DeviceId.hpp>
#include <Tanker/Types/Password.hpp>
#include <Tanker/Types/TrustchainId.hpp>
#include <Tanker/Types/UnlockKey.hpp>
#include <Tanker/Types/UserId.hpp>
#include <Tanker/Unlock/Create.hpp>
#include <Tanker/Unlock/Messages.hpp>
#include <Tanker/Unlock/Registration.hpp>
#include <Tanker/UnverifiedEntry.hpp>
#include <Tanker/UserKeyStore.hpp>
#include <Tanker/UserNotFound.hpp>
#include <Tanker/UserToken/Delegation.hpp>

#include <fmt/format.h>
#include <mpark/variant.hpp>
#include <nlohmann/json.hpp>
#include <tconcurrent/async_wait.hpp>
#include <tconcurrent/future.hpp>
#include <tconcurrent/promise.hpp>
#include <tconcurrent/when.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <stdexcept>
#include <utility>

TLOG_CATEGORY(Session);

namespace Tanker
{
namespace
{

template <typename T, typename F>
auto convertList(std::vector<T> const& source, F&& f)
{
  std::vector<std::result_of_t<F(decltype(*begin(source)))>> ret;
  ret.reserve(source.size());

  std::transform(begin(source), end(source), std::back_inserter(ret), f);
  return ret;
}

std::vector<UserId> obfuscateUserIds(std::vector<SUserId> const& suserIds,
                                     TrustchainId const& trustchainId)
{
  return convertList(suserIds, [&](auto&& suserId) {
    return obfuscateUserId(suserId, trustchainId);
  });
}

std::vector<GroupId> convertToGroupIds(std::vector<SGroupId> const& sgroupIds)
{
  return convertList(sgroupIds, [](auto&& sgroupId) {
    return base64::decode<GroupId>(sgroupId.string());
  });
}

template <typename S, typename T>
auto toClearId(std::vector<T> const& errorIds,
               std::vector<S> const& sIds,
               std::vector<T> const& Ids)
{
  std::vector<S> clearIds;
  clearIds.reserve(Ids.size());

  for (auto const& wrongId : errorIds)
  {
    auto const badIt = std::find(Ids.begin(), Ids.end(), wrongId);

    assert(badIt != Ids.end() && "Wrong id not found");

    clearIds.push_back(sIds[std::distance(Ids.begin(), badIt)]);
  }
  return clearIds;
}
template <typename T>
std::vector<T> removeDuplicates(std::vector<T> stuff)
{
  std::sort(begin(stuff), end(stuff));
  stuff.erase(std::unique(begin(stuff), end(stuff)), end(stuff));
  return stuff;
}
}

Session::Session(Config&& config)
  : _trustchainId(config.trustchainId),
    _userId(config.userId),
    _userSecret(config.userSecret),
    _db(std::move(config.db)),
    _deviceKeyStore(std::move(config.deviceKeyStore)),
    _client(std::move(config.client)),
    _trustchain(_db.get()),
    _userKeyStore(_db.get()),
    _contactStore(_db.get()),
    _groupStore(_db.get()),
    _resourceKeyStore(_db.get()),
    _verifier(_trustchainId, _db.get(), &_contactStore, &_groupStore),
    _trustchainPuller(&_trustchain,
                      &_verifier,
                      _db.get(),
                      _client.get(),
                      _deviceKeyStore->signatureKeyPair().publicKey,
                      _deviceKeyStore->deviceId(),
                      _userId),
    _userAccessor(_userId, &_trustchainPuller, &_contactStore),
    _groupAcessor(&_trustchainPuller, &_groupStore),
    _blockGenerator(_trustchainId,
                    _deviceKeyStore->signatureKeyPair().privateKey,
                    _deviceKeyStore->deviceId())
{
  _client->setConnectionHandler(
      [this]() -> tc::cotask<void> { TC_AWAIT(connectionHandler()); });

  _client->blockAvailable.connect(
      [this] { _trustchainPuller.scheduleCatchUp(); });

  _trustchainPuller.receivedThisDeviceId =
      [this](auto const& deviceId) -> tc::cotask<void> {
    TC_AWAIT(this->setDeviceId(deviceId));
  };
  _trustchainPuller.receivedKeyToDevice =
      [this](auto const& entry) -> tc::cotask<void> {
    TC_AWAIT(this->onKeyToDeviceReceived(entry));
  };
  _trustchainPuller.receivedKeyToUser = [this](auto const& entry) {
    this->onKeyToUserReceived(entry);
  };
  _trustchainPuller.receivedKeyToUserGroup = [this](auto const& entry) {
    this->onKeyToUserGroupReceived(entry);
  };
  _trustchainPuller.deviceCreated =
      [this](auto const& entry) -> tc::cotask<void> {
    TC_AWAIT(onDeviceCreated(entry));
  };
  _trustchainPuller.userGroupActionReceived =
      [this](auto const& entry) -> tc::cotask<void> {
    TC_AWAIT(onUserGroupEntry(entry));
  };
}

tc::cotask<void> Session::connectionHandler()
{
  // NOTE: It is MANDATORY to check this prefix is valid, or the server could
  // get us to sign anything!
  constexpr static const char challengePrefix[] =
      "\U0001F512 Auth Challenge. 1234567890.";
  try
  {
    auto const challenge = TC_AWAIT(_client->requestAuthChallenge());
    if (challenge.size() < sizeof(challengePrefix) ||
        !std::equal(challengePrefix,
                    challengePrefix + sizeof(challengePrefix) - 1,
                    begin(challenge)))
      throw std::runtime_error(
          "Received auth challenge does not contain mandatory prefix. Server "
          "may not be up to date, or we may be under attack.");
    auto const signature =
        Crypto::sign(gsl::make_span(challenge).as_span<uint8_t const>(),
                     _deviceKeyStore->signatureKeyPair().privateKey);
    auto const request = nlohmann::json{
        {"signature", signature},
        {"public_signature_key", _deviceKeyStore->signatureKeyPair().publicKey},
        {"trustchain_id", _trustchainId},
        {"user_id", _userId}};
    _unlockMethods = TC_AWAIT(_client->authenticateDevice(request));
  }
  catch (std::exception const& e)
  {
    TERROR("Failed to authenticate session: {}", e.what());
  }
}

tc::cotask<void> Session::startConnection()
{
  TC_AWAIT(_client->handleConnection());

  tc::async_resumable([this]() -> tc::cotask<void> {
    TC_AWAIT(syncTrustchain());
    _ready.set_value({});
  });

  TC_AWAIT(_ready.get_future());
}

UserId const& Session::userId() const
{
  return this->_userId;
}

TrustchainId const& Session::trustchainId() const
{
  return this->_trustchainId;
}

Crypto::SymmetricKey const& Session::userSecret() const
{
  return this->_userSecret;
}

tc::cotask<void> Session::encrypt(uint8_t* encryptedData,
                                  gsl::span<uint8_t const> clearData,
                                  std::vector<SUserId> const& suserIds,
                                  std::vector<SGroupId> const& sgroupIds)
{
  auto const metadata = Encryptor::encrypt(encryptedData, clearData);
  auto userIds = obfuscateUserIds(suserIds, this->_trustchainId);
  auto groupIds = convertToGroupIds(sgroupIds);
  userIds.insert(userIds.begin(), this->_userId);

  TC_AWAIT(_resourceKeyStore.putKey(metadata.mac, metadata.key));
  TC_AWAIT(share({metadata.mac}, userIds, groupIds));
}

tc::cotask<void> Session::decrypt(uint8_t* decryptedData,
                                  gsl::span<uint8_t const> encryptedData,
                                  std::chrono::steady_clock::duration timeout)
{
  auto const mac = Encryptor::extractMac(encryptedData);
  auto const futures = {TC_AWAIT(waitForKey(mac)),
                        tc::async_wait(timeout).to_shared()};
  auto const result = TC_AWAIT(tc::when_any(
      futures.begin(), futures.end(), tc::when_any_options::auto_cancel));
  if (result.index == 1)
    throw Error::formatEx<Error::ResourceKeyNotFound>(
        fmt("couldn't find key for {:s}"), mac);

  auto const keyPublish = TC_AWAIT(_trustchain.findKeyPublish(mac));
  if (keyPublish)
    TC_AWAIT(ReceiveKey::decryptAndStoreKey(
        _resourceKeyStore, _userKeyStore, _groupStore, *keyPublish));

  auto const key = TC_AWAIT(_resourceKeyStore.getKey(mac));
  Encryptor::decrypt(decryptedData, key, encryptedData);
}

tc::cotask<void> Session::setDeviceId(DeviceId const& deviceId)
{
  TC_AWAIT(_deviceKeyStore->setDeviceId(deviceId));
  _trustchainPuller.setDeviceId(deviceId);
  _blockGenerator.setDeviceId(deviceId);
}

DeviceId const& Session::deviceId() const
{
  return _deviceKeyStore->deviceId();
}

tc::cotask<void> Session::share(std::vector<Crypto::Mac> const& resourceIds,
                                std::vector<UserId> const& userIds,
                                std::vector<GroupId> const& groupIds)
{
  TC_AWAIT(Share::share(_deviceKeyStore->encryptionKeyPair().privateKey,
                        _resourceKeyStore,
                        _userAccessor,
                        _groupAcessor,
                        _blockGenerator,
                        *_client,
                        resourceIds,
                        userIds,
                        groupIds));
}

tc::cotask<void> Session::share(std::vector<SResourceId> const& sresourceIds,
                                std::vector<SUserId> const& suserIds,
                                std::vector<SGroupId> const& sgroupIds)
{
  auto userIds = obfuscateUserIds(suserIds, this->_trustchainId);
  auto groupIds = convertToGroupIds(sgroupIds);
  auto resourceIds = convertList(sresourceIds, [](auto&& resourceId) {
    return base64::decode<Crypto::Mac>(resourceId);
  });

  // we remove ourselves from the recipients
  userIds.erase(
      std::remove_if(begin(userIds),
                     end(userIds),
                     [this](auto&& rec) { return rec == this->_userId; }),
      end(userIds));

  userIds = removeDuplicates(std::move(userIds));
  groupIds = removeDuplicates(std::move(groupIds));
  if (!userIds.empty() || !groupIds.empty())
  {
    try
    {
      TC_AWAIT(share(resourceIds, userIds, groupIds));
    }
    catch (Error::RecipientNotFound const& e)
    {
      auto const clearUids = toClearId(e.userIds(), suserIds, userIds);
      auto const clearGids = toClearId(e.groupIds(), sgroupIds, groupIds);
      throw Error::formatEx<Error::RecipientNotFound>(
          fmt("Unknown users: [{:s}], groups: [{:s}]"),
          fmt::join(clearUids.begin(), clearUids.end(), ", "),
          fmt::join(clearGids.begin(), clearGids.end(), ", "));
    }
  }
}

tc::cotask<SGroupId> Session::createGroup(std::vector<SUserId> suserIds)
{
  suserIds = removeDuplicates(std::move(suserIds));
  auto userIds = obfuscateUserIds(suserIds, _trustchainId);

  try
  {
    auto const groupId = TC_AWAIT(Groups::Manager::create(
        _userAccessor, _blockGenerator, *_client, userIds));
    // Make sure group's lastBlockHash updates before the next group operation
    TC_AWAIT(syncTrustchain());
    TC_RETURN(groupId);
  }
  catch (Error::UserNotFound const& e)
  {
    auto const clearUids = toClearId(e.userIds(), suserIds, userIds);
    throw Error::formatEx<Error::UserNotFound>(
        fmt("Unknown users: {:s}"),
        fmt::join(clearUids.begin(), clearUids.end(), ", "));
  }
  throw std::runtime_error("unreachable code");
}

tc::cotask<void> Session::updateGroupMembers(SGroupId const& groupIdString,
                                             std::vector<SUserId> userIdsToAdd)
{
  auto const groupId = base64::decode<GroupId>(groupIdString);
  userIdsToAdd = removeDuplicates(std::move(userIdsToAdd));
  auto const usersToAdd = obfuscateUserIds(userIdsToAdd, _trustchainId);

  try
  {
    TC_AWAIT(Groups::Manager::updateMembers(_userAccessor,
                                            _blockGenerator,
                                            *_client,
                                            _groupStore,
                                            groupId,
                                            usersToAdd));
  }
  catch (Error::UserNotFound const& e)
  {
    auto const clearUids = toClearId(e.userIds(), userIdsToAdd, usersToAdd);
    throw Error::formatEx<Error::UserNotFound>(
        fmt("Unknown users: {:s}"),
        fmt::join(clearUids.begin(), clearUids.end(), ", "));
  }

  // Make sure group's lastBlockHash updates before the next group operation
  TC_AWAIT(syncTrustchain());
}

tc::cotask<std::unique_ptr<Unlock::Registration>> Session::generateUnlockKey()
{
  TC_RETURN(Unlock::generate(
      _userId, TC_AWAIT(_userKeyStore.getLastKeyPair()), _blockGenerator));
}

tc::cotask<void> Session::registerUnlockKey(
    Unlock::Registration const& registration)
{
  TC_AWAIT(_client->pushBlock(registration.block));
}

tc::cotask<void> Session::createUnlockKey(
    Unlock::CreationOptions const& options)
{
  auto const reg = TC_AWAIT(generateUnlockKey());
  auto const msg = Unlock::Message(
      trustchainId(),
      deviceId(),
      Unlock::UpdateOptions(
          options.get<Email>(), options.get<Password>(), reg->unlockKey),
      userSecret(),
      _deviceKeyStore->signatureKeyPair().privateKey);
  try
  {
    TC_AWAIT(_client->pushBlock(reg->block));
    TC_AWAIT(_client->createUnlockKey(msg));
    updateLocalUnlockMethods(options);
  }
  catch (Error::ServerError const& e)
  {
    if (e.httpStatusCode() == 500)
      throw Error::InternalError(e.what());
    else if (e.httpStatusCode() == 409)
      throw Error::UnlockKeyAlreadyExists(
          "An unlock key has already been registered");
    else
      throw;
  }
}

void Session::updateLocalUnlockMethods(
    Unlock::RegistrationOptions const& options)
{
  if (options.get<Email>().has_value())
    _unlockMethods |= Unlock::Method::Email;
  if (options.get<Password>().has_value())
    _unlockMethods |= Unlock::Method::Password;
}

tc::cotask<void> Session::updateUnlock(Unlock::UpdateOptions const& options)
{
  auto const msg =
      Unlock::Message(trustchainId(),
                      deviceId(),
                      options,
                      userSecret(),
                      _deviceKeyStore->signatureKeyPair().privateKey);
  try
  {
    TC_AWAIT(_client->updateUnlockKey(msg));
    updateLocalUnlockMethods(
        std::forward_as_tuple(options.get<Email>(), options.get<Password>()));
  }
  catch (Error::ServerError const& e)
  {
    if (e.httpStatusCode() == 400)
      throw Error::InvalidUnlockKey{e.what()};
    throw;
  }
}

tc::cotask<void> Session::registerUnlock(
    Unlock::RegistrationOptions const& options)
{
  if (!this->_unlockMethods)
    TC_AWAIT(createUnlockKey(options));
  else
    TC_AWAIT(updateUnlock(Unlock::UpdateOptions{
        options.get<Email>(), options.get<Password>(), nonstd::nullopt}));
}

tc::cotask<UnlockKey> Session::generateAndRegisterUnlockKey()
{
  auto const reg = TC_AWAIT(generateUnlockKey());
  TC_AWAIT(registerUnlockKey(*reg));
  TC_RETURN(reg->unlockKey);
}

tc::cotask<bool> Session::isUnlockAlreadySetUp() const
{
  auto const devices = TC_AWAIT(_contactStore.findUserDevices(_userId));
  TC_RETURN(std::any_of(devices.begin(), devices.end(), [](auto const& device) {
    return device.isGhostDevice;
  }));
}

Unlock::Methods Session::registeredUnlockMethods() const
{
  return _unlockMethods;
}

bool Session::hasRegisteredUnlockMethods() const
{
  return !!_unlockMethods;
}

bool Session::hasRegisteredUnlockMethods(Unlock::Method method) const
{
  return !!(_unlockMethods & method);
}

tc::cotask<void> Session::catchUserKey(DeviceId const& deviceId,
                                       DeviceCreation const& deviceCreation)

{
  // no new user key (or old device < 3), no need to continue
  auto const optUserKeyPair = deviceCreation.userKeyPair();
  if (!optUserKeyPair)
    TC_RETURN();
  // you need this so that Share shares to self using the user key
  TC_AWAIT(_contactStore.putUserKey(deviceCreation.userId(),
                                    optUserKeyPair->publicEncryptionKey));
  if (deviceId == this->deviceId())
  {
    auto const key = Crypto::sealDecrypt<Crypto::PrivateEncryptionKey>(
        optUserKeyPair->encryptedPrivateEncryptionKey,
        _deviceKeyStore->encryptionKeyPair());

    TC_AWAIT(
        _userKeyStore.putPrivateKey(optUserKeyPair->publicEncryptionKey, key));
  }
}

tc::cotask<void> Session::onKeyToDeviceReceived(Entry const& entry)
{
  TC_AWAIT(ReceiveKey::onKeyToDeviceReceived(
      _contactStore,
      _resourceKeyStore,
      _deviceKeyStore->encryptionKeyPair().privateKey,
      entry));
}

tc::cotask<void> Session::onDeviceCreated(Entry const& entry)
{
  auto const& deviceCreation =
      mpark::get<DeviceCreation>(entry.action.variant());
  DeviceId const deviceId{entry.hash};
  TC_AWAIT(catchUserKey(deviceId, deviceCreation));
  Device createdDevice{deviceId,
                       entry.index,
                       nonstd::nullopt,
                       deviceCreation.publicSignatureKey(),
                       deviceCreation.publicEncryptionKey(),
                       deviceCreation.isGhostDevice()};
  TC_AWAIT(_contactStore.putUserDevice(deviceCreation.userId(), createdDevice));
  if (deviceCreation.userId() == userId() && !deviceCreation.isGhostDevice())
    deviceCreated();
}

void Session::onKeyToUserReceived(Entry const& entry)
{
  auto const& keyPublishToUser =
      mpark::get<KeyPublishToUser>(entry.action.variant());
  signalKeyReady(keyPublishToUser.mac);
}

void Session::onKeyToUserGroupReceived(Entry const& entry)
{
  auto const& keyPublishToUserGroup =
      mpark::get<KeyPublishToUserGroup>(entry.action.variant());
  signalKeyReady(keyPublishToUserGroup.resourceId);
}

tc::cotask<void> Session::onUserGroupEntry(Entry const& entry)
{
  TC_AWAIT(GroupUpdater::applyEntry(_groupStore, _userKeyStore, entry));
}

tc::cotask<void> Session::syncTrustchain()
{
  TC_AWAIT(_trustchainPuller.scheduleCatchUp());
}

std::unique_ptr<ChunkEncryptor> Session::makeChunkEncryptor()
{
  return std::make_unique<ChunkEncryptor>(ChunkEncryptor(this));
}

tc::cotask<std::unique_ptr<ChunkEncryptor>> Session::makeChunkEncryptor(
    gsl::span<uint8_t const> encryptedSeal,
    std::chrono::steady_clock::duration timeout)
{
  auto chunkEncryptor = std::make_unique<ChunkEncryptor>(this);
  TC_AWAIT(chunkEncryptor->open(encryptedSeal, timeout));
  TC_RETURN(std::move(chunkEncryptor));
}

void Session::signalKeyReady(Crypto::Mac const& mac)
{
  auto const it = _pendingRequests.find(mac);
  if (it != _pendingRequests.end())
  {
    it->second.set_value({});
    _pendingRequests.erase(it);
  }
}

tc::cotask<tc::shared_future<void>> Session::waitForKey(Crypto::Mac const& mac)
{
  if (TC_AWAIT(_resourceKeyStore.findKey(mac)))
    TC_RETURN(tc::make_ready_future());

  if (TC_AWAIT(_trustchain.findKeyPublish(mac)))
    TC_RETURN(tc::make_ready_future());

  auto const it = _pendingRequests.find(mac);
  if (it != _pendingRequests.end())
    TC_RETURN(it->second.get_future());

  tc::promise<void> prom;
  _pendingRequests[mac] = prom;
  TC_RETURN(prom.get_future());
}
}
