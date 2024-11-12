#ifndef ZERO_FORMATTER_H
#define ZERO_FORMATTER_H

#include <expected>
#include <algorithm>
#include <exception>
#include <fmt/core.h>

template<typename Char, typename T, typename E>
struct fmt::formatter<std::expected<T, E>, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const std::expected<T, E> &expected, FmtContext &ctx) {
        using namespace std::string_view_literals;

        if (!expected)
            return fmt::format_to(ctx.out(), "unexpected({})", expected.error());

        if constexpr (std::is_void_v<T>)
            return std::ranges::copy("expected()"sv, ctx.out()).out;
        else
            return fmt::format_to(ctx.out(), "expected({})", *expected);
    }
};

template<typename Char>
struct fmt::formatter<std::exception_ptr, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const std::exception_ptr &ptr, FmtContext &ctx) {
        using namespace std::string_view_literals;

        if (!ptr)
            return std::ranges::copy("nullptr"sv, ctx.out()).out;

        try {
            std::rethrow_exception(ptr);
        }
        catch (const std::exception &e) {
            return fmt::format_to(ctx.out(), "exception({})", e);
        }
    }
};

#endif //ZERO_FORMATTER_H
