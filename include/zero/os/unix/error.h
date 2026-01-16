#ifndef ZERO_OS_UNIX_ERROR_H
#define ZERO_OS_UNIX_ERROR_H

#include <cstdint>
#include <expected>
#include <system_error>

#undef unix

namespace zero::os::unix {
    template<typename F>
        requires std::is_integral_v<std::invoke_result_t<F>>
    std::expected<std::invoke_result_t<F>, std::error_code> expected(F &&f) {
        const auto result = f();

        if (result == -1)
            return std::unexpected{std::error_code{errno, std::system_category()}};

        return result;
    }

    template<typename F>
        requires std::is_pointer_v<std::invoke_result_t<F>>
    std::expected<std::invoke_result_t<F>, std::error_code> expected(F &&f) {
        const auto result = f();

        if (reinterpret_cast<std::intptr_t>(result) == -1)
            return std::unexpected{std::error_code{errno, std::system_category()}};

        return result;
    }

    template<typename F>
        requires std::is_integral_v<std::invoke_result_t<F>>
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
