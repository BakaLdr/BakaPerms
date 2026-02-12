#pragma once
#include "BakaPerms/StdAfx.hpp"

#include <ll/api/i18n/I18n.h>

#include <filesystem>
#include <stacktrace>
#include <string>
#include <string_view>

using namespace ll::i18n_literals;

namespace BakaPerms::utils::exception {

class BakaException : public std::exception {
public:
    explicit BakaException(std::string message)
    : message_(std::move(message)),
      stackTrace_(std::stacktrace::current()) {}

    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

    void printException() const noexcept {
        logger.error("{}", "bakaperms.exception.header"_tr(what()));
        logger.error("{}", "bakaperms.exception.stacktrace"_tr());

        // #0 = BakaException ctor, #1 = derived ctor, #2 = actual throw site
        constexpr std::size_t skipFrames = 2;
        constexpr std::size_t maxFrames  = 10;

        if (stackTrace_.size() <= skipFrames) return;

        const std::size_t available    = stackTrace_.size() - skipFrames;
        const std::size_t displayCount = std::min(available, maxFrames);

        for (std::size_t i = 0; i < displayCount; ++i) {
            const auto& frame    = stackTrace_[i + skipFrames];
            auto        filename = std::filesystem::path(frame.source_file()).filename().string();
            logger.error(
                "{}",
                "bakaperms.exception.stack_frame"_trl(
                    "",
                    frame.description(),
                    filename.empty() ? "unknown" : filename,
                    frame.source_line()
                )
            );
        }

        if (available > maxFrames) {
            logger.error("{}", "bakaperms.exception.stack_collapsed"_tr(available - maxFrames));
        }
    }

private:
    std::string     message_;
    std::stacktrace stackTrace_;
};

class OperationFailedException final : public BakaException {
public:
    explicit OperationFailedException(std::string_view detail)
    : BakaException("bakaperms.exception.operation_failed"_tr(detail)) {}
};

class InvalidArgumentException final : public BakaException {
public:
    explicit InvalidArgumentException(std::string_view detail)
    : BakaException("bakaperms.exception.invalid_argument"_tr(detail)) {}
};

} // namespace BakaPerms::utils::exception
