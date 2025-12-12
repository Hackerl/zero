#ifndef ZERO_EXPECT_H
#define ZERO_EXPECT_H

#include <expected>

#define Z_EXPECT(...)                                               \
    if (auto &&_result = __VA_ARGS__; !_result)                     \
        return std::unexpected{std::move(_result).error()}

#define Z_CO_EXPECT(...)                                            \
    if (auto &&_result = __VA_ARGS__; !_result)                     \
        co_return std::unexpected{std::move(_result).error()}

#ifdef __GNUC__
#define Z_TRY(...)                                                  \
    ({                                                              \
        auto &&_result = __VA_ARGS__;                               \
                                                                    \
        if (!_result)                                               \
            return std::unexpected{std::move(_result).error()};     \
                                                                    \
        *std::move(_result);                                        \
    })
#endif

#ifdef __clang__
#define Z_CO_TRY(...)                                               \
    ({                                                              \
        auto &&_result = __VA_ARGS__;                               \
                                                                    \
        if (!_result)                                               \
            co_return std::unexpected{std::move(_result).error()};  \
                                                                    \
        *std::move(_result);                                        \
    })
#endif

#endif //ZERO_EXPECT_H
