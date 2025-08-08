#ifndef ZERO_FORMATTER_H
#define ZERO_FORMATTER_H

#include <algorithm>
#include <exception>
#include <fmt/core.h>

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
