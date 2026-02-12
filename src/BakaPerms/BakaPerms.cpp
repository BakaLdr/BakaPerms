#include "BakaPerms/BakaPerms.hpp"

#include "BakaPerms/Commands/PermsCommand.hpp"
#include "BakaPerms/Config.hpp"
#include "BakaPerms/Database/SQLite/SQLiteDatabase.h"
#include "BakaPerms/Utils/I18n/I18n.hpp"

#include <ll/api/event/EventBus.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/PlayerInfo.h>

#include <filesystem>
#include <memory>

namespace BakaPerms {

BakaPerms& BakaPerms::getInstance() {
    static BakaPerms instance;
    return instance;
}

bool BakaPerms::load() {
    utils::i18n::I18nManager::getInstance().init();
    try {
        // Ensure PlayerInfo service is initialized before we start loading permissions
        ll::service::PlayerInfo::getInstance();

        config::load();

        const auto dataDir = getSelf().getDataDir();
        std::filesystem::create_directories(dataDir);

        auto dbPath = dataDir / "permissions.db";
        if (config::config.Database.Type == "sqlite") {
            dbPath       = dataDir / config::config.Database.SQLite.Path;
            auto db      = std::make_unique<database::SQLiteDatabase>(dbPath);
            mPermManager = std::make_unique<core::PermissionManager>(std::move(db));
        } else if (/*config::config.Database.Type == "postgresql"*/ true) {
            throw utils::exception::InvalidArgumentException(
                "bakaperms.error.unsupported_db"_tr(config::config.Database.Type)
            );
        }
        return true;
    } catch (utils::exception::BakaException& e) {
        e.printException();
        return false;
    } catch (const std::exception& e) {
        logger.error("{}", "bakaperms.error.load_failed"_tr(e.what()));
        return false;
    }
}

bool BakaPerms::enable() {
    auto& eventBus = ll::event::EventBus::getInstance();

    mPlayerDisconnectListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
        [this](const ll::event::PlayerDisconnectEvent& event) {
            const auto& player = event.self();
            const auto  uuid   = player.getUuid().asString();
            mPermManager->invalidatePlayer(uuid);
        }
    );

    commands::registerCommands();

    return true;
}

bool BakaPerms::disable() {
    auto& eventBus = ll::event::EventBus::getInstance();

    if (mPlayerDisconnectListener) {
        eventBus.removeListener(mPlayerDisconnectListener);
        mPlayerDisconnectListener = nullptr;
    }

    if (mPermManager) {
        mPermManager->invalidateAll();
    }

    return true;
}

bool BakaPerms::unload() {
    mPermManager.reset();
    return true;
}

} // namespace BakaPerms

LL_REGISTER_MOD(BakaPerms::BakaPerms, BakaPerms::BakaPerms::getInstance());
