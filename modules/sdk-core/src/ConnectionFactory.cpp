#include <Tanker/ConnectionFactory.hpp>

#ifndef EMSCRIPTEN
#include <Tanker/Connection.hpp>
#else
#include <Tanker/JsConnection.hpp>
#endif

namespace Tanker
{
ConnectionPtr ConnectionFactory::create(std::string url,
                                        nonstd::optional<SdkInfo> info)
{
#ifndef EMSCRIPTEN
  return std::make_unique<Connection>(std::move(url), std::move(info));
#else
  return std::make_unique<JsConnection>(url);
#endif
}
}
