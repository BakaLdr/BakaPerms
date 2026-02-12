#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace BakaPerms::database {

struct DbNull {
    bool operator==(const DbNull&) const = default;
};

using DbValue   = std::variant<DbNull, int, std::int64_t, double, std::string>;
using ParamList = std::vector<DbValue>;

class Row {
    std::vector<DbValue> columns_;

public:
    explicit Row(std::vector<DbValue> columns) : columns_(std::move(columns)) {}

    [[nodiscard]] auto getInt(const std::size_t index) const -> int {
        const auto& val = columns_.at(index);
        if (const auto* p = std::get_if<int>(&val)) return *p;
        if (const auto* p = std::get_if<std::int64_t>(&val)) return static_cast<int>(*p);
        throw std::bad_variant_access();
    }

    [[nodiscard]] auto getInt64(const std::size_t index) const -> std::int64_t {
        const auto& val = columns_.at(index);
        if (const auto* p = std::get_if<std::int64_t>(&val)) return *p;
        if (const auto* p = std::get_if<int>(&val)) return *p;
        throw std::bad_variant_access();
    }

    [[nodiscard]] auto getDouble(const std::size_t index) const -> double {
        const auto& val = columns_.at(index);
        if (const auto* p = std::get_if<double>(&val)) return *p;
        if (const auto* p = std::get_if<int>(&val)) return *p;
        if (const auto* p = std::get_if<std::int64_t>(&val)) return static_cast<double>(*p);
        throw std::bad_variant_access();
    }

    [[nodiscard]] auto getString(const std::size_t index) const -> const std::string& {
        return std::get<std::string>(columns_.at(index));
    }

    [[nodiscard]] bool isNull(const std::size_t index) const {
        return std::holds_alternative<DbNull>(columns_.at(index));
    }

    [[nodiscard]] auto columnCount() const noexcept -> std::size_t { return columns_.size(); }
};

using ResultSet = std::vector<Row>;

} // namespace BakaPerms::database
