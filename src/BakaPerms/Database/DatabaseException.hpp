#pragma once

#include "BakaPerms/Utils/Exception/Exceptions.hpp"

#include <string>

namespace BakaPerms::database {

enum class DbErrorCode { None, ConnectionFailed, QueryFailed, BindError, ConstraintViolation, Unknown };

class DatabaseException : public utils::exception::BakaException {
    DbErrorCode code_;

public:
    explicit DatabaseException(const DbErrorCode code, const std::string& message)
    : BakaException(message),
      code_(code) {}

    [[nodiscard]] DbErrorCode code() const noexcept { return code_; }
};

} // namespace BakaPerms::database
