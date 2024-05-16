#ifndef ZERO_NT_ERROR_H
#define ZERO_NT_ERROR_H

#include <optional>
#include <windows.h>
#include <fmt/core.h>
#include <zero/error.h>

namespace zero::os::nt {
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
}

DECLARE_ERROR_CODE(zero::os::nt::ResultHandle)

#endif //ZERO_NT_ERROR_H
