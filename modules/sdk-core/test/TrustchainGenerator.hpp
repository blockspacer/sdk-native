
#include <Tanker/Crypto/SealedPrivateSignatureKey.hpp>
#include <Tanker/Crypto/SignatureKeyPair.hpp>
#include <Tanker/DeviceKeys.hpp>
#include <Tanker/Groups/Group.hpp>
#include <Tanker/Identity/SecretProvisionalIdentity.hpp>
#include <Tanker/Identity/TargetType.hpp>
#include <Tanker/ProvisionalUsers/PublicUser.hpp>
#include <Tanker/ProvisionalUsers/SecretUser.hpp>
#include <Tanker/Trustchain/ClientEntry.hpp>
#include <Tanker/Trustchain/Context.hpp>
#include <Tanker/Trustchain/ResourceId.hpp>
#include <Tanker/Trustchain/ServerEntry.hpp>
#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/Types/ProvisionalUserKeys.hpp>
#include <Tanker/Users/LocalUser.hpp>
#include <Tanker/Users/User.hpp>

#include <deque>
#include <optional>
#include <vector>

namespace Tanker::Test
{

struct Device : Users::Device
{
  Device(Trustchain::ClientEntry entry,
         Trustchain::UserId const& uid,
         DeviceKeys const& deviceKeys,
         bool isGhostDevice = true);
  Crypto::PrivateEncryptionKey privateEncryptionKey;
  Crypto::PrivateSignatureKey privateSignatureKey;
  Trustchain::ClientEntry entry;
  DeviceKeys keys() const;
};

struct User;
class ProvisionalUser;
class Resource;

struct Group
{
  // Group v2
  Group(Trustchain::TrustchainId const& tid,
        Device const& author,
        std::vector<User> const& users,
        std::vector<ProvisionalUser> const& provisionalUsers);

  // Group v1
  Group(Trustchain::TrustchainId const& tid,
        Device const& author,
        std::vector<User> const& users);

  operator Tanker::InternalGroup() const;
  explicit operator Tanker::ExternalGroup() const;
  Trustchain::GroupId const& id() const;
  Crypto::EncryptionKeyPair const& currentEncKp() const;
  Crypto::SignatureKeyPair const& currentSigKp() const;
  Crypto::SealedPrivateSignatureKey encryptedSignatureKey() const;
  Crypto::Hash lastBlockHash() const;

  std::vector<Trustchain::ClientEntry> const& entries() const;

  Trustchain::ClientEntry const& addUsers(
      Device const& author,
      std::vector<User> const& users = {},
      std::vector<ProvisionalUser> const& provisionalUsers = {});

  Trustchain::ClientEntry const& addUsersV1(Device const& author,
                                            std::vector<User> const& users);

private:
  Trustchain::TrustchainId _tid;
  Crypto::EncryptionKeyPair _currentEncKp;
  Crypto::SignatureKeyPair _currentSigKp;
  Trustchain::GroupId _id;
  std::vector<Trustchain::ClientEntry> _entries;
};

struct User
{
  User(Trustchain::UserId const& id,
       Trustchain::TrustchainId const& tid,
       std::optional<Crypto::EncryptionKeyPair> userKey,
       gsl::span<Device const> devices);

  User(User const&) = default;
  User& operator=(User const&) = default;
  User(User&&) = default;
  User& operator=(User&&) = default;

  operator Users::User() const;
  operator Users::LocalUser() const;

  Trustchain::UserId const& id() const;
  std::vector<Crypto::EncryptionKeyPair> const& userKeys() const;
  std::vector<Trustchain::ClientEntry> entries() const;
  std::deque<Device> const& devices() const;
  std::deque<Device>& devices();
  Trustchain::ClientEntry revokeDevice(Device& target);
  Trustchain::ClientEntry revokeDeviceV1(Device& target);
  Trustchain::ClientEntry revokeDeviceForMigration(Device const& sender,
                                                   Device& target);

  [[nodiscard]] Device makeDevice() const;
  Device& addDevice();

  [[nodiscard]] Device makeDeviceV1() const;
  Device& addDeviceV1();

  Group makeGroup(
      std::vector<User> const& users = {},
      std::vector<ProvisionalUser> const& provisionalUsers = {}) const;

  Trustchain::ClientEntry claim(ProvisionalUser const& provisionalUser) const;

  Crypto::EncryptionKeyPair const& addUserKey();
  void addUserKey(Crypto::EncryptionKeyPair const& userKp);

private:
  Trustchain::UserId _id;
  Trustchain::TrustchainId _tid;
  std::vector<Crypto::EncryptionKeyPair> _userKeys;
  std::deque<Device> _devices;
};

class ProvisionalUser
{

public:
  ProvisionalUser(Trustchain::TrustchainId const& tid, std::string value);

  operator ProvisionalUsers::PublicUser() const;
  operator ProvisionalUsers::SecretUser() const;
  operator ProvisionalUserKeys() const;
  operator Identity::SecretProvisionalIdentity() const;

  Crypto::EncryptionKeyPair const& appEncryptionKeyPair() const;
  Crypto::EncryptionKeyPair const& tankerEncryptionKeyPair() const;
  Crypto::SignatureKeyPair const& appSignatureKeyPair() const;
  Crypto::SignatureKeyPair const& tankerSignatureKeyPair() const;

private:
  Trustchain::TrustchainId _tid;
  Identity::TargetType _target;
  std::string _value;
  Crypto::EncryptionKeyPair _appEncKp;
  Crypto::EncryptionKeyPair _tankerEncKp;
  Crypto::SignatureKeyPair _appSigKp;
  Crypto::SignatureKeyPair _tankerSigKp;
};

class Resource
{
public:
  inline auto const& id() const
  {
    return _rid;
  }

  inline auto const& key() const
  {
    return _key;
  }

  Resource();
  Resource(Trustchain::ResourceId const& id, Crypto::SymmetricKey const& key);
  [[nodiscard]] bool operator==(Resource const& rhs) const noexcept;

  Resource(Resource const&) = delete;
  Resource(Resource&&) = delete;
  Resource& operator=(Resource const&) = delete;
  Resource& operator=(Resource&&) = delete;

private:
  Trustchain::ResourceId _rid;
  Crypto::SymmetricKey _key;
};

class Generator
{
public:
  Generator();

  User makeUser(std::string const& suserId) const;
  User makeUserV1(std::string const& suserId) const;

  Group makeGroup(
      Device const& author,
      std::vector<User> const& users = {},
      std::vector<ProvisionalUser> const& provisionalUsers = {}) const;

  Group makeGroupV1(Device const& author, std::vector<User> const& users) const;

  ProvisionalUser makeProvisionalUser(std::string const& email);

  Trustchain::ClientEntry shareWith(Device const& sender,
                                    User const& receiver,
                                    Resource const& res);
  Trustchain::ClientEntry shareWith(Device const& sender,
                                    Group const& receiver,
                                    Resource const& res);
  Trustchain::ClientEntry shareWith(Device const& sender,
                                    ProvisionalUser const& receiver,
                                    Resource const& res);

  Trustchain::Context const& context() const;
  Trustchain::ServerEntry const& rootBlock() const;
  Crypto::SignatureKeyPair const& trustchainSigKp() const;
  static std::vector<Trustchain::ServerEntry> makeEntryList(
      std::vector<Trustchain::ClientEntry> const& entries);
  static std::vector<Trustchain::ServerEntry> makeEntryList(
      std::initializer_list<Device> devices);
  std::vector<Trustchain::ServerEntry> makeEntryList(
      std::initializer_list<User> users) const;

private:
  Crypto::SignatureKeyPair _trustchainKeyPair;
  Trustchain::ServerEntry _rootBlock;
  Trustchain::Context _context;
};
}