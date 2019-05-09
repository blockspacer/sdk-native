#pragma once

#include <Tanker/Trustchain/Actions/KeyPublish/ToDevice.hpp>
#include <Tanker/Trustchain/Actions/KeyPublish/ToProvisionalUser.hpp>
#include <Tanker/Trustchain/Actions/KeyPublish/ToUser.hpp>
#include <Tanker/Trustchain/Actions/KeyPublish/ToUserGroup.hpp>
#include <Tanker/Trustchain/Actions/Nature.hpp>
#include <Tanker/Trustchain/Preprocessor/Actions/VariantImplementation.hpp>
#include <Tanker/Trustchain/ResourceId.hpp>

#include <mpark/variant.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>

namespace Tanker
{
namespace Trustchain
{
namespace Actions
{
#define TANKER_TRUSTCHAIN_ACTIONS_KEY_PUBLISH_ATTRIBUTES \
  (resourceId, ResourceId)

class KeyPublish
{
  TANKER_TRUSTCHAIN_ACTION_VARIANT_IMPLEMENTATION(
      KeyPublish,
      (KeyPublishToDevice,
       KeyPublishToUser,
       KeyPublishToUserGroup,
       KeyPublishToProvisionalUser),
      TANKER_TRUSTCHAIN_ACTIONS_KEY_PUBLISH_ATTRIBUTES)

public:
  using ToDevice = KeyPublishToDevice;
  using ToUser = KeyPublishToUser;
  using ToUserGroup = KeyPublishToUserGroup;
  using ToProvisionalUser = KeyPublishToProvisionalUser;

  Nature nature() const;
};

// The nature is not present in the wired payload.
// Therefore there is no from_serialized overload for KeyPublish.
std::uint8_t* to_serialized(std::uint8_t*, KeyPublish const&);
std::size_t serialized_size(KeyPublish const&);

void to_json(nlohmann::json&, KeyPublish const&);
}
}
}
