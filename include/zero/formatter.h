#ifndef ZERO_FORMATTER_H
#define ZERO_FORMATTER_H

#include <fmt/core.h>
#include <tl/expected.hpp>

template<typename Char, typename T, typename E>
struct fmt::formatter<tl::expected<T, E>, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    auto format(const tl::expected<T, E> &expected, FmtContext &ctx) const {
        if (!expected)
            return fmt::format_to(ctx.out(), "unexpected({})", expected.error());

        if constexpr (std::is_void_v<T>) {
            return std::ranges::copy(std::string_view{"expected()"}, ctx.out()).out;
        } else {
            return fmt::format_to(ctx.out(), "expected({})", *expected);
        }
    }
};

#endif //ZERO_FORMATTER_H
