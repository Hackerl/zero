#ifndef ZERO_UNIX_ERROR_H
#define ZERO_UNIX_ERROR_H

#include <system_error>
#include <tl/expected.hpp>

#undef unix

namespace zero::os::unix {
    template<typename F>
    tl::expected<std::invoke_result_t<F>, std::error_code> expected(F &&f) {
        const auto result = f();

        if (result == -1)
            return tl::unexpected(std::error_code(errno, std::system_category()));

        return result;
    }

    template<typename F, std::enable_if_t<std::is_integral_v<std::invoke_result_t<F>>>* = nullptr>
    tl::expected<std::invoke_result_t<F>, std::error_code> ensure(F &&f) {
        while (true) {
            const auto result = expected(std::forward<F>(f));

            if (result)
                return *result;

            if (result.error() != std::errc::interrupted)
                return tl::unexpected(result.error());
        }
    }
}

#endif //ZERO_UNIX_ERROR_H
