#include <Tanker/Trustchain/UserId.hpp>
#include <Tanker/Users/User.hpp>
#include <Tanker/Users/UserAccessor.hpp>

#include <Helpers/Await.hpp>
#include <Helpers/MakeCoTask.hpp>

#include "TrustchainGenerator.hpp"
#include "UserRequesterStub.hpp"

#include <doctest.h>
#include <trompeloeil.hpp>

using namespace Tanker;

TEST_CASE("UserAccessor")
{
  Test::Generator generator;
  auto const alice = generator.makeUser("alice");
  auto const bob = generator.makeUser("bob");
  auto const charlie = generator.makeUser("charlie");

  UserRequesterStub requester;
  Users::UserAccessor userAccessor(generator.context(), &requester);

  SUBCASE("it should return user ids it did not find")
  {
    REQUIRE_CALL(requester, getUsers(ANY(gsl::span<Trustchain::UserId const>)))
        .RETURN(makeCoTask(std::vector<Trustchain::ServerEntry>{}));

    std::vector ids{bob.id(), charlie.id()};
    auto const result = AWAIT(userAccessor.pull(ids));
    CHECK_UNARY(result.found.empty());
    CHECK_EQ(result.notFound, ids);
  }

  SUBCASE("it should return found users")
  {
    std::vector ids{alice.id(), bob.id(), charlie.id()};

    REQUIRE_CALL(requester, getUsers(ids))
        .RETURN(makeCoTask(generator.makeEntryList({alice, bob, charlie})));
    auto result = AWAIT(userAccessor.pull(ids));
    CHECK_UNARY(result.notFound.empty());
    auto expectedUsers = std::vector<Users::User>{alice, bob, charlie};

    std::sort(result.found.begin(), result.found.end());
    std::sort(expectedUsers.begin(), expectedUsers.end());
    CHECK_EQ(result.found, expectedUsers);
  }
}
