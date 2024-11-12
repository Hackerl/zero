#ifndef ZERO_UTILITY_H
#define ZERO_UTILITY_H

#include <tl/expected.hpp>
#include <zero/detail/type_traits.h>

namespace zero {
    template<typename T, typename E1, typename E2, std::enable_if_t<std::is_convertible_v<E1, E2>>* = nullptr>
    auto flatten(tl::expected<tl::expected<T, E1>, E2> expected) {
        return std::move(expected).and_then([](auto result) -> tl::expected<T, E2> {
            return std::move(result).transform_error([](auto error) -> E2 {
                return error;
            });
        });
    }

    template<typename E, typename T, std::enable_if_t<detail::is_specialization_v<T, tl::expected>>* = nullptr>
    auto flattenWith(T expected) {
        return flatten(std::move(expected).transform_error([](auto error) -> E {
            return error;
        }));
    }
}

#endif //ZERO_UTILITY_H
