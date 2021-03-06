#include <Tanker/Trustchain/Action.hpp>

#include <Tanker/Serialization/Serialization.hpp>
#include <Tanker/Trustchain/Errors/Errc.hpp>

#include <Helpers/Errors.hpp>

#include <doctest.h>

using namespace Tanker;
using namespace Tanker::Trustchain;
using namespace Tanker::Trustchain::Actions;

TEST_CASE("Action tests")
{
  Action dc{DeviceCreation{}};

  SUBCASE("Action variant functions")
  {
    CHECK(dc.holds_alternative<DeviceCreation>());
    CHECK(dc.get_if<DeviceCreation>() != nullptr);
    CHECK(dc.get_if<DeviceRevocation>() == nullptr);
    CHECK_NOTHROW(dc.get<DeviceCreation>());
    CHECK_THROWS_AS(dc.get<DeviceRevocation>(),
                    boost::variant2::bad_variant_access);
  }

  SUBCASE("Serialization")
  {
    auto const payload = Serialization::serialize(DeviceCreation{});

    CHECK(Serialization::serialize(dc) == payload);
    CHECK(Action::deserialize(Nature::DeviceCreation, payload) == dc);

    TANKER_CHECK_THROWS_WITH_CODE(
        Action::deserialize(static_cast<Nature>(-1), payload),
        Errc::InvalidBlockNature);
  }
}
