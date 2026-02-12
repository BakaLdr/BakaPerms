#pragma once

#include "BakaPerms/Database/IDatabase.hpp"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include <filesystem>
#include <memory>
#include <mutex>

namespace BakaPerms::database {

class SQLiteDatabase final : public IDatabase {
public:
    explicit SQLiteDatabase(const std::filesystem::path& dbPath);
    ~SQLiteDatabase() override;

    void exec(std::string_view sql) override;
    auto execute(std::string_view sql, const ParamList& params) -> int override;
    auto query(std::string_view sql, const ParamList& params) -> ResultSet override;
    auto queryOne(std::string_view sql, const ParamList& params) -> std::optional<Row> override;
    bool exists(std::string_view sql, const ParamList& params) override;
    void withTransaction(const std::function<void()>& fn) override;

private:
    static void bindParams(SQLite::Statement& stmt, const ParamList& params);
    static auto extractRow(const SQLite::Statement& stmt) -> Row;

    std::unique_ptr<SQLite::Database> db_;
    std::recursive_mutex              mutex_;
};

} // namespace BakaPerms::database
