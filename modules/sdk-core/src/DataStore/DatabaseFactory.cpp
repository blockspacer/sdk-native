#include <Tanker/DataStore/DatabaseFactory.hpp>

#ifndef EMSCRIPTEN
#include <Tanker/DataStore/Database.hpp>
#else
#include <Tanker/DataStore/JsDatabase.hpp>
#endif

#include <Tanker/Log/Log.hpp>

#include <Tanker/Tracer/ScopeTimer.hpp>

TLOG_CATEGORY("DatabaseFactory");

namespace Tanker
{
namespace DataStore
{
tc::cotask<DatabasePtr> createDatabase(
    std::string const& dbPath,
    std::optional<Crypto::SymmetricKey> const& userSecret,
    bool exclusive)
{
  FUNC_TIMER(DB);
#ifndef EMSCRIPTEN
  auto db = std::make_unique<Database>(dbPath, userSecret, exclusive);
  TC_AWAIT(db->migrate());
  TC_RETURN(std::move(db));
#else
  auto db = std::make_unique<JsDatabase>();
  TC_AWAIT(db->open(dbPath));
  TC_RETURN(std::move(db));
#endif
}
}
}
