#include <Tanker/Trustchain/Actions/UserGroupAddition.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Crypto/PrivateSignatureKey.hpp>
#include <Tanker/Serialization/Serialization.hpp>
#include <Tanker/Trustchain/UserId.hpp>

#include <Helpers/Buffers.hpp>

#include <doctest.h>

using namespace Tanker;
using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;

TEST_CASE("UserGroupAddition1 tests")
{
  SUBCASE("selfSign should return the selfSignature")
  {
    auto const signatureKeyPair = Crypto::makeSignatureKeyPair();
    UserGroupAddition::v1 uga{};
    auto const& signature = uga.selfSign(signatureKeyPair.privateKey);
    CHECK(signature == uga.selfSignature());
  }
}

TEST_CASE("Serialization test vectors")
{
  SUBCASE("it should serialize/deserialize a UserGroupAddition1")
  {
    // clang-format off
    std::vector<std::uint8_t> const serializedUserGroupAddition = {
      // group id
      0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x69, 0x64, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // previous group block hash
      0x70, 0x72, 0x65, 0x76, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x62,
      0x6c, 0x6f, 0x63, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // varint
      0x02,
      // public user encryption key 1
      0x70, 0x75, 0x62,
      0x20, 0x75, 0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      // encrypted group private encryption key 1
      0x65, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74,
      0x65, 0x64, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x70, 0x72, 0x69,
      0x76, 0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
      // public user encryption key 2
      0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x20, 0x70, 0x75, 0x62, 0x20,
      0x75, 0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // encrypted group private encryption key 2
      0x73, 0x65, 0x63,
      0x6f, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x65,
      0x64, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x70, 0x72, 0x69, 0x76,
      0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      // self signature
      0x73, 0x65, 0x6c, 0x66, 0x20, 0x73, 0x69,
      0x67, 0x6e, 0x61, 0x74, 0x75, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // clang-format on

    auto const groupId = make<GroupId>("group id");
    auto const previousGroupBlockHash = make<Crypto::Hash>("prev group block");
    UserGroupAddition::v1::SealedPrivateEncryptionKeysForUsers const
        sealedPrivateEncryptionKeysForUsers{
            {make<Crypto::PublicEncryptionKey>("pub user key"),
             make<Crypto::SealedPrivateEncryptionKey>(
                 "encrypted group priv key")},
            {make<Crypto::PublicEncryptionKey>("second pub user key"),
             make<Crypto::SealedPrivateEncryptionKey>(
                 "second encrypted group priv key")}};
    auto const selfSignature = make<Crypto::Signature>("self signature");

    UserGroupAddition::v1 const uga{groupId,
                                    previousGroupBlockHash,
                                    sealedPrivateEncryptionKeysForUsers,
                                    selfSignature};

    CHECK(Serialization::serialize(uga) == serializedUserGroupAddition);
    CHECK(Serialization::deserialize<UserGroupAddition::v1>(
              serializedUserGroupAddition) == uga);
  }

  SUBCASE("it should serialize/deserialize a UserGroupAddition2")
  {
    // clang-format off
    std::vector<std::uint8_t> const serializedUserGroupAddition = {
      // group id
      0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x69, 0x64, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // previous group block hash
      0x70, 0x72, 0x65, 0x76, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x62,
      0x6c, 0x6f, 0x63, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // varint
      0x02,
      // User ID 1
      0x75, 0x73, 0x65, 0x72, 0x20, 0x69, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // public user encryption key 1
      0x70, 0x75, 0x62, 0x20, 0x75, 0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // encrypted group private encryption key 1
      0x65, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x65, 0x64, 0x20, 0x67, 0x72,
      0x6f, 0x75, 0x70, 0x20, 0x70, 0x72, 0x69, 0x76, 0x20, 0x6b, 0x65, 0x79,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // User ID 2
      0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x20, 0x75, 0x73, 0x65, 0x72, 0x20,
      0x69, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // public user encryption key 2
      0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x20, 0x70, 0x75, 0x62, 0x20, 0x75,
      0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // encrypted group private encryption key 2
      0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x63, 0x72, 0x79,
      0x70, 0x74, 0x65, 0x64, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x70,
      0x72, 0x69, 0x76, 0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Varint
      0x02,
      // public app encryption key 1
      0x61, 0x70, 0x70, 0x20, 0x70, 0x72, 0x6f, 0x76, 0x69, 0x73, 0x69, 0x6f,
      0x6e, 0x61, 0x6c, 0x20, 0x75, 0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // public tanker encryption key 1
      0x74, 0x61, 0x6e, 0x6b, 0x65, 0x72, 0x20, 0x70, 0x72, 0x6f, 0x76, 0x69,
      0x73, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x20, 0x75, 0x73, 0x65, 0x72, 0x20,
      0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00,
      // encrypted group private encryption key 1
      0x70, 0x72, 0x6f, 0x76, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x20,
      0x65, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x65, 0x64, 0x20, 0x67, 0x72,
      0x6f, 0x75, 0x70, 0x20, 0x70, 0x72, 0x69, 0x76, 0x20, 0x6b, 0x65, 0x79,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // provisional user app encryption key 2
      0x32, 0x6e, 0x64, 0x20, 0x61, 0x70, 0x70, 0x20, 0x70, 0x72, 0x6f, 0x76,
      0x69, 0x73, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x20, 0x75, 0x73, 0x65, 0x72,
      0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00,
      // provisional user tanker encryption key 2
      0x32, 0x6e, 0x64, 0x20, 0x74, 0x61, 0x6e, 0x6b, 0x65, 0x72, 0x20, 0x70,
      0x72, 0x6f, 0x76, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x20, 0x75,
      0x73, 0x65, 0x72, 0x20, 0x6b, 0x65, 0x79, 0x00,
      // encrypted group private encryption key 2
      0x32, 0x6e, 0x64, 0x20, 0x70, 0x72, 0x6f, 0x76, 0x69, 0x73, 0x69, 0x6f,
      0x6e, 0x61, 0x6c, 0x20, 0x65, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x65,
      0x64, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x70, 0x72, 0x69, 0x76,
      0x20, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // self signature
      0x73, 0x65, 0x6c, 0x66, 0x20, 0x73, 0x69, 0x67, 0x6e, 0x61, 0x74, 0x75,
      0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00
    };
    // clang-format on

    auto const groupId = make<GroupId>("group id");
    auto const previousGroupBlockHash = make<Crypto::Hash>("prev group block");
    std::vector<UserGroupAddition::v2::Member> const members{
        {make<UserId>("user id"),
         make<Crypto::PublicEncryptionKey>("pub user key"),
         make<Crypto::SealedPrivateEncryptionKey>("encrypted group priv key")},
        {make<UserId>("second user id"),
         make<Crypto::PublicEncryptionKey>("second pub user key"),
         make<Crypto::SealedPrivateEncryptionKey>(
             "second encrypted group priv key")}};
    std::vector<UserGroupAddition::v2::ProvisionalMember> const
        provisionalMembers{
            {make<Crypto::PublicSignatureKey>("app provisional user key"),
             make<Crypto::PublicSignatureKey>("tanker provisional user key"),
             make<Crypto::TwoTimesSealedPrivateEncryptionKey>(
                 "provisional encrypted group priv key")},
            {make<Crypto::PublicSignatureKey>("2nd app provisional user key"),
             make<Crypto::PublicSignatureKey>(
                 "2nd tanker provisional user key"),
             make<Crypto::TwoTimesSealedPrivateEncryptionKey>(
                 "2nd provisional encrypted group priv key")}};
    auto const selfSignature = make<Crypto::Signature>("self signature");

    UserGroupAddition::v2 const uga{groupId,
                                    previousGroupBlockHash,
                                    members,
                                    provisionalMembers,
                                    selfSignature};

    CHECK(Serialization::serialize(uga) == serializedUserGroupAddition);
    CHECK(Serialization::deserialize<UserGroupAddition::v2>(
              serializedUserGroupAddition) == uga);
  }
}
