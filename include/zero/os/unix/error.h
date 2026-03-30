#ifndef ZERO_OS_UNIX_ERROR_H
#define ZERO_OS_UNIX_ERROR_H

#include <cstdint>
#include <expected>
#include <functional>
#include <system_error>
#include <zero/meta/concepts.h>

#undef unix

namespace zero::os::unix {
    template<typename F>
    concept GeneralSystemAPI =
        requires(F &&f) { { std::invoke(std::forward<F>(f)) } -> std::integral; } ||
        requires(F &&f) { { std::invoke(std::forward<F>(f)) } -> meta::Pointer; };

    template<GeneralSystemAPI F>
    std::expected<std::invoke_result_t<F>, std::error_code> expected(F &&f) {
        using T = std::invoke_result_t<F>;

        const auto result = std::invoke(std::forward<F>(f));

        if constexpr (std::is_integral_v<T>) {
            if (result == -1)
                return std::unexpected{std::error_code{errno, std::system_category()}};
        }
        else {
            if (reinterpret_cast<std::intptr_t>(result) == -1)
                return std::unexpected{std::error_code{errno, std::system_category()}};
        }

        return result;
    }

    template<GeneralSystemAPI F>
    std::expected<std::invoke_result_t<F>, std::error_code> ensure(F &&f) {
        while (true) {
            const auto result = expected(std::forward<F>(f));

            if (result)
                return *result;

            if (const auto &error = result.error(); error != std::errc::interrupted)
                return std::unexpected{error};
        }
    }
}

#endif //ZERO_OS_UNIX_ERROR_H
