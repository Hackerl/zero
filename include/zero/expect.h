#ifndef ZERO_EXPECT_H
#define ZERO_EXPECT_H

#define EXPECT(...)                                                 \
    if (auto &&_result = __VA_ARGS__; !_result)                     \
        return tl::unexpected(std::move(_result).error())

#define CO_EXPECT(...)                                              \
    if (auto &&_result = __VA_ARGS__; !_result)                     \
        co_return tl::unexpected(std::move(_result).error())

#ifdef __GNUC__
#define TRY(...)                                                    \
    ({                                                              \
        auto &&_result = __VA_ARGS__;                               \
                                                                    \
        if (!_result)                                               \
            return tl::unexpected(std::move(_result).error());      \
                                                                    \
        *std::move(_result);                                        \
    })
#endif

#ifdef __clang__
#define CO_TRY(...)                                                 \
    ({                                                              \
        auto &&_result = __VA_ARGS__;                               \
                                                                    \
        if (!_result)                                               \
            co_return tl::unexpected(std::move(_result).error());   \
                                                                    \
        *std::move(_result);                                        \
    })
#endif

#endif //ZERO_EXPECT_H
