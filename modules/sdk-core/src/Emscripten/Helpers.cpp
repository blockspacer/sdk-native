#include <Tanker/Emscripten/Helpers.hpp>

#include <Tanker/Emscripten/Error.hpp>

#include <Tanker/Errors/Errc.hpp>
#include <Tanker/Errors/Exception.hpp>

#include <cppcodec/base64_rfc4648.hpp>

using OneArgFunction = std::function<void(emscripten::val const&)>;
using PromiseCallback = std::function<void(emscripten::val, emscripten::val)>;

namespace Tanker
{
namespace Emscripten
{
std::vector<uint8_t> copyToVector(emscripten::val const& typedArray)
{
  using emscripten::val;

  auto const length = typedArray["length"].as<unsigned int>();
  std::vector<uint8_t> vec(length);

  val memory = val::module_property("buffer");
  val memoryView = typedArray["constructor"].new_(
      memory, reinterpret_cast<uintptr_t>(vec.data()), length);

  memoryView.call<void>("set", typedArray);

  return vec;
}

tc::cotask<emscripten::val> jsPromiseToFuture(emscripten::val const& jspromise)
{
  tc::promise<emscripten::val> cpppromise;
  auto const thenCb = OneArgFunction([=](emscripten::val const& value) mutable {
    cpppromise.set_value(value);
  });
  auto const catchCb =
      OneArgFunction([=](emscripten::val const& error) mutable {
        std::string errorMsg;
        if (error.isNull())
          errorMsg = "null";
        else if (error.isUndefined())
          errorMsg = "undefined";
        else
          errorMsg = error.call<std::string>("toString") + "\n" +
                     error["stack"].as<std::string>();
        cpppromise.set_exception(std::make_exception_ptr(Errors::formatEx(
            Errors::Errc::InternalError, "JS error: {}", errorMsg)));
      });
  jspromise.call<emscripten::val>("then", toJsFunctionObject(thenCb))
      .call<emscripten::val>("catch", toJsFunctionObject(catchCb));
  TC_RETURN(TC_AWAIT(cpppromise.get_future()));
}
}
}

EMSCRIPTEN_BINDINGS(jshelpers)
{
  emscripten::class_<OneArgFunction>("NoargOrMaybeMoreFunction")
      .constructor<>()
      .function("opcall", &OneArgFunction::operator());

  emscripten::class_<PromiseCallback>("PromiseCallback")
      .constructor<>()
      .function("opcall", &PromiseCallback::operator());
}
