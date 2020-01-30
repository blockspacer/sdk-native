#pragma once

#include <Tanker/Crypto/PrivateEncryptionKey.hpp>
#include <Tanker/ProvisionalUsers/IAccessor.hpp>
#include <Tanker/Trustchain/Actions/KeyPublish.hpp>

#include <tconcurrent/coroutine.hpp>

namespace Tanker
{

namespace Groups
{
class IAccessor;
}

namespace Users
{
class ILocalUserAccessor;
}

class ResourceKeyStore;

namespace ReceiveKey
{
tc::cotask<void> decryptAndStoreKey(
    ResourceKeyStore& resourceKeyStore,
    Users::ILocalUserAccessor& localUserAccessor,
    Groups::IAccessor& GroupAccessor,
    ProvisionalUsers::IAccessor& provisionalUsersAccessor,
    Trustchain::Actions::KeyPublish const& kp);
}
}
