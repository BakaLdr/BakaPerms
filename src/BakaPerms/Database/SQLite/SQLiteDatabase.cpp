#include "BakaPerms/Database/SQLite/SQLiteDatabase.h"

#include "BakaPerms/Database/DatabaseException.hpp"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

constexpr int kSqliteInteger = 1;
constexpr int kSqliteFloat   = 2;
constexpr int kSqliteText    = 3;
constexpr int kSqliteBlob    = 4;
constexpr int kSqliteNull    = 5;

namespace BakaPerms::database {

SQLiteDatabase::SQLiteDatabase(const std::filesystem::path& dbPath) {
    try {
        db_ = std::make_unique<SQLite::Database>(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db_->exec("PRAGMA journal_mode=WAL");
        db_->exec("PRAGMA foreign_keys=ON");
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::ConnectionFailed, e.what());
    }
}

SQLiteDatabase::~SQLiteDatabase() = default;

void SQLiteDatabase::exec(const std::string_view sql) {
    std::lock_guard lock(mutex_);
    try {
        db_->exec(std::string(sql));
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::QueryFailed, e.what());
    }
}

int SQLiteDatabase::execute(const std::string_view sql, const ParamList& params) {
    std::lock_guard lock(mutex_);
    try {
        SQLite::Statement stmt(*db_, std::string(sql));
        bindParams(stmt, params);
        return stmt.exec();
    } catch (const DatabaseException&) {
        throw;
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::QueryFailed, e.what());
    }
}

ResultSet SQLiteDatabase::query(const std::string_view sql, const ParamList& params) {
    std::lock_guard lock(mutex_);
    try {
        SQLite::Statement stmt(*db_, std::string(sql));
        bindParams(stmt, params);
        ResultSet results;
        while (stmt.executeStep()) {
            results.push_back(extractRow(stmt));
        }
        return results;
    } catch (const DatabaseException&) {
        throw;
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::QueryFailed, e.what());
    }
}

std::optional<Row> SQLiteDatabase::queryOne(const std::string_view sql, const ParamList& params) {
    std::lock_guard lock(mutex_);
    try {
        SQLite::Statement stmt(*db_, std::string(sql));
        bindParams(stmt, params);
        if (stmt.executeStep()) {
            return extractRow(stmt);
        }
        return std::nullopt;
    } catch (const DatabaseException&) {
        throw;
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::QueryFailed, e.what());
    }
}

bool SQLiteDatabase::exists(const std::string_view sql, const ParamList& params) {
    std::lock_guard lock(mutex_);
    try {
        SQLite::Statement stmt(*db_, std::string(sql));
        bindParams(stmt, params);
        return stmt.executeStep();
    } catch (const DatabaseException&) {
        throw;
    } catch (const SQLite::Exception& e) {
        throw DatabaseException(DbErrorCode::QueryFailed, e.what());
    }
}

void SQLiteDatabase::withTransaction(const std::function<void()>& fn) {
    std::lock_guard lock(mutex_);
    try {
        db_->exec("BEGIN TRANSACTION");
        fn();
        db_->exec("COMMIT");
    } catch (...) {
        try {
            db_->exec("ROLLBACK");
        } catch (...) {}
        throw;
    }
}

void SQLiteDatabase::bindParams(SQLite::Statement& stmt, const ParamList& params) {
    for (std::size_t i = 0; i < params.size(); ++i) {
        const int idx = static_cast<int>(i) + 1;
        std::visit(
            [&stmt, idx]<typename Ty>(const Ty& val) {
                using T = std::decay_t<Ty>;
                if constexpr (std::is_same_v<T, DbNull>) {
                    stmt.bind(idx);
                } else {
                    stmt.bind(idx, val);
                }
            },
            params[i]
        );
    }
}

Row SQLiteDatabase::extractRow(const SQLite::Statement& stmt) {
    const int            count = stmt.getColumnCount();
    std::vector<DbValue> columns;
    columns.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        switch (const auto col = stmt.getColumn(i); col.getType()) {
        case kSqliteInteger:
            columns.emplace_back(col.getInt64());
            break;
        case kSqliteFloat:
            columns.emplace_back(col.getDouble());
            break;
        case kSqliteText:
            columns.emplace_back(std::string(col.getString()));
            break;
        case kSqliteNull:
        default:
            columns.emplace_back(DbNull{});
            break;
        }
    }
    return Row(std::move(columns));
}

} // namespace BakaPerms::database
