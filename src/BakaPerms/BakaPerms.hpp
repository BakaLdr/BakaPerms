#pragma once
#include "BakaPerms/Core/PermissionManager.hpp"
#include "BakaPerms/Utils/Macros.h"

#include <ll/api/event/ListenerBase.h>
#include <ll/api/mod/NativeMod.h>

#include <memory>

namespace BakaPerms {

class BakaPerms {

public:
    BAKA_PERMAPI static BakaPerms& getInstance();

    BakaPerms() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    [[nodiscard]] core::IPermissionManager& getPermissionManager() const { return *mPermManager; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod&                      mSelf;
    std::shared_ptr<core::PermissionManager> mPermManager;
    ll::event::ListenerPtr                   mPlayerDisconnectListener;
};

} // namespace BakaPerms
