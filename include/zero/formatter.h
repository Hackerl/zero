#ifndef ZERO_FORMATTER_H
#define ZERO_FORMATTER_H

#include <fmt/std.h>
#include <tl/expected.hpp>

template<typename Char, typename T, typename E>
struct fmt::formatter<tl::expected<T, E>, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const tl::expected<T, E> &expected, FmtContext &ctx) {
        using namespace std::string_view_literals;

        if (!expected)
            return fmt::format_to(ctx.out(), "unexpected({})", expected.error());

        if constexpr (std::is_void_v<T>) {
            constexpr auto result = "expected()"sv;
            return std::copy(result.begin(), result.end(), ctx.out());
        }
        else {
            return fmt::format_to(ctx.out(), "expected({})", *expected);
        }
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

        if (!ptr) {
            constexpr auto result = "nullptr"sv;
            return std::copy(result.begin(), result.end(), ctx.out());
        }

        try {
            std::rethrow_exception(ptr);
        }
        catch (const std::exception &e) {
            return fmt::format_to(ctx.out(), "exception({})", e);
        }
    }
};

#endif //ZERO_FORMATTER_H
