#include <Tanker/ResourceKeys/Store.hpp>

#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/DataStore/ADatabase.hpp>
#include <Tanker/Errors/Errc.hpp>

#include <Helpers/Await.hpp>
#include <Helpers/Buffers.hpp>
#include <Helpers/Errors.hpp>

#include <cppcodec/base64_rfc4648.hpp>
#include <doctest.h>

using namespace Tanker;

#ifndef EMSCRIPTEN
#include <Tanker/DataStore/Connection.hpp>
#include <Tanker/DataStore/Table.hpp>
#include <Tanker/DataStore/Utils.hpp>
#include <Tanker/DbModels/ResourceKeys.hpp>

namespace
{
struct OldResourceKeys
{
  std::string b64Mac;
  std::string b64ResourceKey;
};

OldResourceKeys setupResourceKeysMigration(DataStore::Connection& db)
{
  auto const resourceKey = Crypto::makeSymmetricKey();

  auto const b64Mac =
      cppcodec::base64_rfc4648::encode(make<Trustchain::ResourceId>("michel"));
  auto const b64ResourceKey = cppcodec::base64_rfc4648::encode(resourceKey);

  db.execute(R"(
    CREATE TABLE resource_keys (
      id INTEGER PRIMARY KEY,
      mac TEXT NOT NULL,
      resource_key TEXT NOT NULL
    );
  )");

  db.execute(fmt::format("INSERT INTO resource_keys VALUES (1, '{}', '{}')",
                         b64Mac,
                         b64ResourceKey));

  return {b64Mac, b64ResourceKey};
}
}
#endif

TEST_CASE("Resource Keys Store")
{
  auto const dbPtr = AWAIT(DataStore::createDatabase(":memory:"));

  ResourceKeys::Store keys(dbPtr.get());

  SUBCASE("it should not find a non-existent key")
  {
    auto const unexistentMac = make<Trustchain::ResourceId>("unexistent");

    TANKER_CHECK_THROWS_WITH_CODE(AWAIT(keys.getKey(unexistentMac)),
                                  Errors::Errc::InvalidArgument);
  }

  SUBCASE("it should find a key that was inserted")
  {
    auto const resourceId = make<Trustchain::ResourceId>("mymac");
    auto const key = make<Crypto::SymmetricKey>("mykey");

    AWAIT_VOID(keys.putKey(resourceId, key));
    auto const key2 = AWAIT(keys.getKey(resourceId));

    CHECK(key == key2);
  }

  SUBCASE("passing a list of resourceIds")
  {
    auto const resourceId1 = make<Trustchain::ResourceId>("mymac1");
    auto const key1 = make<Crypto::SymmetricKey>("mykey1");
    AWAIT_VOID(keys.putKey(resourceId1, key1));

    auto const resourceId2 = make<Trustchain::ResourceId>("mymac2");
    auto const key2 = make<Crypto::SymmetricKey>("mykey2");
    AWAIT_VOID(keys.putKey(resourceId2, key2));

    SUBCASE("should return a list of keys")
    {
      auto const result =
          AWAIT(keys.getKeys(std::vector{resourceId1, resourceId2}));
      CHECK(result.size() == 2);
      CHECK(result.at(0) == std::tie(key1, resourceId1));
      CHECK(result.at(1) == std::tie(key2, resourceId2));
    }
    SUBCASE("should throw if some doesn't exist")
    {
      TANKER_CHECK_THROWS_WITH_CODE(
          AWAIT(keys.getKeys(
              std::vector{resourceId1,
                          resourceId2,
                          make<Trustchain::ResourceId>("notmymac")})),
          Errors::Errc::InvalidArgument);
    }
  }

  SUBCASE("it should ignore a duplicate key and keep the first")
  {
    auto const resourceId = make<Trustchain::ResourceId>("mymac");
    auto const key = make<Crypto::SymmetricKey>("mykey");
    auto const key2 = make<Crypto::SymmetricKey>("mykey2");

    AWAIT_VOID(keys.putKey(resourceId, key));
    AWAIT_VOID(keys.putKey(resourceId, key2));
    auto const gotKey = AWAIT(keys.getKey(resourceId));

    CHECK_EQ(key, gotKey);
  }
}

#ifndef EMSCRIPTEN
TEST_CASE("Migration")
{
  auto const dbPtr = DataStore::createConnection(":memory:");
  auto& db = *dbPtr;

  SUBCASE("Migration from version 1 should convert from base64")
  {
    using ResourceKeysTable = Tanker::DbModels::resource_keys::resource_keys;
    ResourceKeysTable tab{};

    auto const oldKeys = setupResourceKeysMigration(db);

    DataStore::createTable<ResourceKeysTable>(db);
    DataStore::migrateTable<ResourceKeysTable>(db, 1);
    auto const keys = db(select(all_of(tab)).from(tab).unconditionally());
    auto const& resourceKeys = keys.front();

    auto const resourceId =
        DataStore::extractBlob<Trustchain::ResourceId>(resourceKeys.mac);
    auto const key =
        DataStore::extractBlob<Crypto::SymmetricKey>(resourceKeys.resource_key);

    CHECK_EQ(resourceId,
             cppcodec::base64_rfc4648::decode<Trustchain::ResourceId>(
                 oldKeys.b64Mac));
    CHECK_EQ(key,
             cppcodec::base64_rfc4648::decode<Crypto::SymmetricKey>(
                 oldKeys.b64ResourceKey));
  }
}
#endif
