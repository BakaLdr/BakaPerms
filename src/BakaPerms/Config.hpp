#pragma once
#include "Utils/Exception/Exceptions.hpp"
#include <ll/api/Config.h>

#include <string>

namespace BakaPerms::config {

void load();
void save();

inline auto configFilePath = BakaPerms::getInstance().getSelf().getConfigDir() / "config.json";

struct ConfigV1 {
    int  version = 1;
    bool enabled = true;
    struct Database {
        std::string Type = "sqlite"; // "sqlite" or "postgresql"
        struct SQLite {
            std::string Path = "permissions.db"; // relative path from dataDir, or absolute
        } SQLite;
        struct PostgreSQL {
            std::string Host     = "localhost";
            int         Port     = 5432;
            std::string Username = "postgres";
            std::string Password;
            std::string Database = "baka_perms";
        } PostgreSQL;
    } Database;
};

using Config = ConfigV1;

inline Config config;

inline void load() {
    if (ll::config::loadConfig(config, configFilePath)) {
        save();
    }
}

inline void save() {
    if (!ll::config::saveConfig(config, configFilePath)) {
        throw utils::exception::OperationFailedException("bakaperms.exception.detail.save_config"_tr());
    }
}


} // namespace BakaPerms::config