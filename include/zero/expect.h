#ifndef ZERO_EXPECT_H
#define ZERO_EXPECT_H

#include <tl/expected.hpp>

#define EXPECT(name)                                                            \
    if (!name)                                                                  \
        return tl::unexpected(std::move(name.error()))

#define CO_EXPECT(name)                                                         \
    if (!name)                                                                  \
        co_return tl::unexpected(std::move(name.error()))

#ifdef __GNUC__
#define TRY(...)                                                                \
    ({                                                                          \
        auto _result = __VA_ARGS__;                                             \
                                                                                \
        if (!_result)                                                           \
            return tl::unexpected(std::move(_result.error()));                  \
                                                                                \
        [](auto v) {                                                            \
            if constexpr (!std::is_void_v<typename decltype(v)::value_type>)    \
                return std::move(*v);                                           \
        }(std::move(_result));                                                  \
    })
#endif

#ifdef __clang__
#define CO_TRY(...)                                                             \
    ({                                                                          \
        auto _result = __VA_ARGS__;                                             \
                                                                                \
        if (!_result)                                                           \
            co_return tl::unexpected(std::move(_result.error()));               \
                                                                                \
        [](auto v) {                                                            \
            if constexpr (!std::is_void_v<typename decltype(v)::value_type>)    \
                return std::move(*v);                                           \
        }(std::move(_result));                                                  \
    })
#endif

#endif //ZERO_EXPECT_H
