#ifndef ZERO_UTILITY_H
#define ZERO_UTILITY_H

#include <expected>
#include <zero/detail/type_traits.h>

namespace zero {
    template<typename T, typename E1, typename E2>
        requires std::is_convertible_v<E1, E2>
    auto flatten(std::expected<std::expected<T, E1>, E2> expected) {
        return std::move(expected).and_then([](auto result) -> std::expected<T, E2> {
            return result;
        });
    }

    template<typename E, typename T>
        requires detail::is_specialization_v<T, std::expected>
    auto flattenWith(T expected) {
        return flatten(std::move(expected).transform_error([](auto error) -> E {
            return error;
        }));
    }
}

#endif //ZERO_UTILITY_H
