#include <Tanker/Errors/AssertionError.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <emscripten/bind.h>

#include <tconcurrent/coroutine.hpp>

namespace Tanker
{
namespace Emscripten
{
inline bool isNone(emscripten::val const& v)
{
  return v.isNull() || v.isUndefined();
}

template <typename T>
std::optional<T> optionalStringFromValue(emscripten::val const& val,
                                            std::string const& key)
{
  if (Emscripten::isNone(val) || Emscripten::isNone(val[key]))
    return std::nullopt;
  else
    return T{val[key].as<std::string>()};
}

template <typename T>
std::optional<T> optionalFromValue(emscripten::val const& val,
                                      std::string const& key)
{
  if (Emscripten::isNone(val) || Emscripten::isNone(val[key]))
    return std::nullopt;
  else
    return val[key].as<T>();
}

template <typename T>
emscripten::val containerToJs(T const& cont)
{
  using emscripten::val;

  auto const Uint8Array = val::global("Uint8Array");
  val memory = val::module_property("buffer");
  return Uint8Array.new_(
      memory, reinterpret_cast<uintptr_t>(cont.data()), cont.size());
}

std::vector<uint8_t> copyToVector(emscripten::val const& typedArray);

template <typename T>
std::vector<T> copyToStringLikeVector(emscripten::val const& typedArray)
{
  using emscripten::val;

  auto const length = typedArray["length"].as<unsigned int>();
  std::vector<T> vec(length);

  for (unsigned int i = 0; i < length; ++i)
    vec[i] = T(typedArray[i].as<std::string>());
  return vec;
}

template <typename Sig>
emscripten::val toJsFunctionObject(std::function<Sig> functor)
{
  return emscripten::val(functor)["opcall"].template call<emscripten::val>(
      "bind", emscripten::val(functor));
}

tc::cotask<emscripten::val> jsPromiseToFuture(emscripten::val const& jspromise);

namespace detail
{
template <template <typename> class F>
inline void resolveJsPromise(emscripten::val resolve, F<void> fut)
{
  fut.get();
  resolve();
}

template <template <typename> class F, typename T>
inline void resolveJsPromise(emscripten::val resolve, F<T> fut)
{
  resolve(fut.get());
}
}

emscripten::val currentExceptionToJs();

template <typename F>
emscripten::val tcFutureToJsPromise(F fut)
{
  auto const Promise = emscripten::val::global("Promise");

  auto resolve = emscripten::val::undefined();
  auto reject = emscripten::val::undefined();
  auto const promise =
      Promise.new_(toJsFunctionObject<void(emscripten::val, emscripten::val)>(
          [&](emscripten::val presolve, emscripten::val preject) {
            resolve = presolve;
            reject = preject;
          }));

  fut.then([resolve, reject](auto fut) mutable {
    try
    {
      detail::resolveJsPromise(resolve, std::move(fut));
    }
    catch (...)
    {
      reject(currentExceptionToJs());
    }
  });

  return promise;
}

template <typename F>
emscripten::val tcExpectedToJsValue(F fut)
{
  assert(fut.is_ready());

  if (!fut.has_exception())
    return emscripten::val(fut.get());
  else
  {
    try
    {
      fut.get();
      throw Errors::AssertionError("unreachable code");
    }
    catch (...)
    {
      return currentExceptionToJs();
    }
  }
}
}
}
