#ifndef ZERO_WINDOWS_ERROR_H
#define ZERO_WINDOWS_ERROR_H

#include <optional>
#include <windows.h>
#include <fmt/core.h>
#include <zero/error.h>
#include <tl/expected.hpp>

namespace zero::os::windows {
    DEFINE_ERROR_TRANSFORMER_EX(
        ResultHandle,
        "HRESULT",
        [](const int value) -> std::string {
            if (HRESULT_FACILITY(value) != FACILITY_WIN32)
                return fmt::format("unknown HRESULT {}", value);

            return std::system_category().message(value);
        },
        [](const int value) -> std::optional<std::error_condition> {
            if (HRESULT_FACILITY(value) != FACILITY_WIN32)
                return std::nullopt;

            return std::system_category().default_error_condition(HRESULT_CODE(value));
        }
    )

    template<typename F, std::enable_if_t<std::is_same_v<std::invoke_result_t<F>, BOOL> || std::is_same_v<std::invoke_result_t<F>, bool>>* = nullptr>
    tl::expected<void, std::error_code> expected(F &&f) {
        const auto result = f();

        if (!result)
            return tl::unexpected(std::error_code(static_cast<int>(GetLastError()), std::system_category()));

        return {};
    }
}

DECLARE_ERROR_CODE(zero::os::windows::ResultHandle)

#endif //ZERO_WINDOWS_ERROR_H
