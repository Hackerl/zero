#ifndef ZERO_UNIX_ERROR_H
#define ZERO_UNIX_ERROR_H

#include <system_error>
#include <tl/expected.hpp>

#undef unix

namespace zero::os::unix {
    template<typename F>
    tl::expected<std::invoke_result_t<F>, std::error_code> expect(F &&f) {
        const auto result = f();

        if (result == -1)
            return tl::unexpected<std::error_code>(errno, std::system_category());

        return result;
    }

    template<typename F>
    tl::expected<std::invoke_result_t<F>, std::error_code> ensure(F &&f) {
        while (true) {
            const auto result = expect(std::forward<F>(f));

            if (!result) {
                if (result.error() == std::errc::interrupted)
                    continue;

                return tl::unexpected(result.error());
            }

            return *result;
        }
    }
}

#endif //ZERO_UNIX_ERROR_H
