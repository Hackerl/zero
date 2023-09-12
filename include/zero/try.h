#ifndef ZERO_TRY_H
#define ZERO_TRY_H

#ifdef __clang__
#define TRY(...)                                        \
    ({                                                  \
        auto _result = __VA_ARGS__;                     \
                                                        \
        if (!_result)                                   \
            return tl::unexpected(_result.error());     \
                                                        \
        std::move(_result);                             \
    })

#define CO_TRY(...)                                     \
    ({                                                  \
        auto _result = __VA_ARGS__;                     \
                                                        \
        if (!_result)                                   \
            co_return tl::unexpected(_result.error());  \
                                                        \
        std::move(_result);                             \
    })
#else
#include <optional>

template<typename T>
struct LastUnexpectedError {
    static std::optional<T> &getInstance() {
        thread_local std::optional<T> instance = {};
        return instance;
    }
};

#define TRY(...)                                                                        \
    [](auto _result) {                                                                  \
        if (!_result)                                                                   \
            LastUnexpectedError<std::error_code>::getInstance() = _result.error();      \
                                                                                        \
        return _result;                                                                 \
    }(__VA_ARGS__);                                                                     \
                                                                                        \
    if (auto &instance = LastUnexpectedError<std::error_code>::getInstance(); instance) \
        return tl::unexpected(*std::exchange(instance, std::nullopt))

#define CO_TRY(...)                                                                     \
    [](auto _result) {                                                                  \
        if (!_result)                                                                   \
            LastUnexpectedError<std::error_code>::getInstance() = _result.error();      \
                                                                                        \
        return _result;                                                                 \
    }(__VA_ARGS__);                                                                     \
                                                                                        \
    if (auto &instance = LastUnexpectedError<std::error_code>::getInstance(); instance) \
        co_return tl::unexpected(*std::exchange(instance, std::nullopt))
#endif

#endif //ZERO_TRY_H
