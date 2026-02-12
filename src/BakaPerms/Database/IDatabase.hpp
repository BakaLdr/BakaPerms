#pragma once

#include "BakaPerms/Database/DbTypes.hpp"

#include <functional>
#include <optional>
#include <string_view>

namespace BakaPerms::database {

class IDatabase {
public:
    virtual ~IDatabase()                   = default;
    IDatabase(const IDatabase&)            = delete;
    IDatabase& operator=(const IDatabase&) = delete;

    /// Execute raw DDL (CREATE TABLE, etc.)
    virtual void exec(std::string_view sql) = 0;

    /// Execute DML with parameters; returns affected row count.
    virtual auto execute(std::string_view sql, const ParamList& params) -> int = 0;

    /// Execute SELECT and return all matching rows.
    virtual auto query(std::string_view sql, const ParamList& params) -> ResultSet = 0;

    /// Execute SELECT and return only the first row, or std::nullopt.
    virtual auto queryOne(std::string_view sql, const ParamList& params) -> std::optional<Row> = 0;

    /// Return true if the SELECT yields at least one row.
    virtual bool exists(std::string_view sql, const ParamList& params) = 0;

    /// Execute a function within a transaction. Commits on success, rolls back on exception.
    virtual void withTransaction(const std::function<void()>& fn) = 0;

    // Non-virtual convenience overloads (no params).
    auto execute(const std::string_view sql) -> int { return execute(sql, {}); }
    auto query(const std::string_view sql) -> ResultSet { return query(sql, {}); }
    auto queryOne(const std::string_view sql) -> std::optional<Row> { return queryOne(sql, {}); }
    bool exists(const std::string_view sql) { return exists(sql, {}); }

protected:
    IDatabase() = default;
};

} // namespace BakaPerms::database
