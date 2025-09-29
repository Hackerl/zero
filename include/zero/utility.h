#ifndef ZERO_UTILITY_H
#define ZERO_UTILITY_H

#include <ctime>
#include <expected>
#include <optional>
#include <zero/detail/type_traits.h>

namespace zero {
    template<typename T>
    std::optional<T> flatten(std::optional<std::optional<T>> optional) {
        if (!optional || !*optional)
            return std::nullopt;

        return *std::move(*optional);
    }

    template<typename T, typename E1, typename E2>
        requires std::is_convertible_v<E1, E2>
    auto flatten(std::expected<std::expected<T, E1>, E2> expected) {
        return std::move(expected).and_then([](std::expected<T, E1> &&result) -> std::expected<T, E2> {
            return result;
        });
    }

    template<typename E, typename T>
        requires detail::is_specialization_v<T, std::expected>
    auto flattenWith(T expected) {
        return flatten(std::move(expected).transform_error([](typename T::error_type &&error) -> E {
            return error;
        }));
    }

    std::tm localTime(std::time_t time);
}

#endif //ZERO_UTILITY_H
