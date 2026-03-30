#ifndef ZERO_UTILITY_H
#define ZERO_UTILITY_H

#include <ctime>
#include <expected>
#include <optional>

namespace zero {
    template<typename T>
    std::optional<T> flatten(std::optional<std::optional<T>> optional) {
        if (!optional || !*optional)
            return std::nullopt;

        return *std::move(*optional);
    }

    template<typename T, typename E1, typename E2>
        requires std::convertible_to<E1, E2>
    std::expected<T, E2> flatten(std::expected<std::expected<T, E1>, E2> expected) {
        return std::move(expected).and_then([](std::expected<T, E1> &&result) -> std::expected<T, E2> {
            return result;
        });
    }

    template<typename U, typename T, typename E>
    auto flattenWith(std::expected<T, E> expected) {
        return flatten(std::move(expected).transform_error([](E &&error) -> U {
            return error;
        }));
    }

    template<typename T, typename E>
        requires (!std::is_void_v<T>)
    std::optional<T> extract(std::expected<T, E> expected) {
        if (!expected)
            return std::nullopt;

        return *std::move(expected);
    }

    template<typename T, typename E>
    std::optional<std::expected<T, E>> transpose(std::expected<std::optional<T>, E> expected) {
        if (!expected)
            return std::unexpected{std::move(expected).error()};

        return *std::move(expected);
    }

    template<typename T, typename E>
        requires (!std::is_void_v<T>)
    std::expected<std::optional<T>, E> transpose(std::optional<std::expected<T, E>> optional) {
        if (!optional)
            return std::nullopt;

        return *std::move(optional);
    }

    std::tm localTime(std::time_t time);
}

#endif //ZERO_UTILITY_H
