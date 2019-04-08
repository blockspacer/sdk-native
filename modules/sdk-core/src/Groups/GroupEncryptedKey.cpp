#include <Tanker/Groups/GroupEncryptedKey.hpp>

#include <cstddef>

namespace Tanker
{
void from_serialized(Serialization::SerializedSource& ss,
                     GroupEncryptedKey& keys)
{
  Serialization::deserialize(ss, keys.publicUserEncryptionKey);
  Serialization::deserialize(ss, keys.encryptedGroupPrivateEncryptionKey);
}

std::uint8_t* to_serialized(std::uint8_t* it, GroupEncryptedKey const& key)
{
  it = Serialization::serialize(it, key.publicUserEncryptionKey);
  return Serialization::serialize(it, key.encryptedGroupPrivateEncryptionKey);
}

std::size_t serialized_size(GroupEncryptedKey const& keys)
{
  return Serialization::serialized_size(keys.publicUserEncryptionKey) +
         Serialization::serialized_size(
             keys.encryptedGroupPrivateEncryptionKey);
}

bool operator==(GroupEncryptedKey const& lhs, GroupEncryptedKey const& rhs)
{
  return std::tie(lhs.publicUserEncryptionKey,
                  lhs.encryptedGroupPrivateEncryptionKey) ==
         std::tie(rhs.publicUserEncryptionKey,
                  rhs.encryptedGroupPrivateEncryptionKey);
}

bool operator!=(GroupEncryptedKey const& lhs, GroupEncryptedKey const& rhs)
{
  return !(lhs == rhs);
}

void to_json(nlohmann::json& j, GroupEncryptedKey const& keys)
{
  j["publicUserEncryptionKey"] = keys.publicUserEncryptionKey;
  j["encryptedGroupPrivateEncryptionKey"] =
      keys.encryptedGroupPrivateEncryptionKey;
}
}
