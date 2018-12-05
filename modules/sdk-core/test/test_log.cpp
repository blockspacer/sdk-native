#include <doctest.h>

#include <Tanker/Crypto/JsonFormat.hpp>
#include <Tanker/Crypto/KeyFormat.hpp>
#include <Tanker/Crypto/base64.hpp>
#include <Tanker/EnumFormat.hpp>
#include <Tanker/Error.hpp>
#include <Tanker/Log.hpp>
#include <Tanker/Nature.hpp>
#include <Tanker/Status.hpp>

#include <Helpers/Buffers.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

TLOG_CATEGORY(test);

namespace
{
void myLogHandler(char const* cat, char level, char const* msg)
{
  std::cout << " this my log handler " << level << " \"" << msg << '"';
}
}

TEST_CASE("print a formated log")
{
  using namespace fmt::literals;
  std::string err = "this is a vary naughty error";
  Log::setLogHandler(nullptr);

  SUBCASE("Print a log")
  {
    TLOG(Info, "this a log");
    TLOG(Info, "this a '{:^26s}' log", "formatted");
  }

  SUBCASE("Set a loghandler")
  {
    Log::setLogHandler(&myLogHandler);
    TINFO("I am the message");
  }

  SUBCASE("Reset a LogHandler")
  {
    Log::setLogHandler(&myLogHandler);
    TINFO("I am the message");
    Log::setLogHandler(nullptr);
    TINFO("I am the message no handler");
  }

  SUBCASE("Print a simple info")
  {
    TINFO("didn't find the error");
  }

  SUBCASE("Print a formated info")
  {
    TINFO(
        "didn't find the error {0}!, "
        "with {1:+02d}, "
        "and {2:.3f} '{0:^6s}'",
        "wat",
        42,
        1.4);
  }

  SUBCASE("Print a status")
  {
    CHECK_EQ(
        fmt::format("this is is a Status {:e}", Tanker::Status::DeviceCreation),
        R"!(this is is a Status 3 DeviceCreation)!");
    CHECK_EQ(
        fmt::format("this is is a Status {}", Tanker::Status::DeviceCreation),
        R"!(this is is a Status 3 DeviceCreation)!");
  }

  SUBCASE("Print a Nature")
  {
    CHECK_EQ(fmt::format("this is is a Nature {:e}",
                         Tanker::Nature::KeyPublishToUser),
             R"!(this is is a Nature 8 KeyPublishToUser)!");
  }

  SUBCASE("Print the fear")
  {
    TERROR("This is bad");
  }

  SUBCASE("Throw an ex")
  {
    REQUIRE_THROWS(throw Tanker::Error::formatEx<std::runtime_error>(
        fmt("You lost, score {:d}/{:f}"), 42, 2.1));
  }

  SUBCASE("It format a json object")
  {
    auto const json = nlohmann::json{{"key", "value"}};
    CHECK_EQ(fmt::format("my json {}", json), R"!(my json {"key":"value"})!");
    CHECK_EQ(fmt::format("my json {:}", json), R"!(my json {"key":"value"})!");
    CHECK_EQ(fmt::format("my json {:j}", json),
             R"!(my json {"key":"value"})!");
    CHECK_EQ(fmt::format("{:5j}", json),
             R"!({
     "key": "value"
})!");
  }

  SUBCASE("It format a Mac")
  {
    auto mac = Tanker::make<Tanker::Crypto::Mac>("awesome, isn't it?");
    REQUIRE(fmt::format("my mac is {}", Tanker::base64::encode(mac)) ==
            fmt::format("my mac is {}", mac));
  }
}
