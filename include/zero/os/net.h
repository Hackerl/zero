#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <span>
#include <array>
#include <string>
#include <vector>
#include <variant>
#include <system_error>
#include <tl/expected.hpp>
#include <fmt/format.h>

namespace zero::os::net {
    struct IfAddress4 {
        std::array<std::byte, 4> ip;
        std::array<std::byte, 4> mask;
    };

    struct IfAddress6 {
        std::array<std::byte, 16> ip;
    };

    using Address = std::variant<IfAddress4, IfAddress6>;

    struct Interface {
        std::string name;
        std::array<std::byte, 6> mac;
        std::vector<Address> addresses;
    };

    std::string stringify(std::span<const std::byte, 4> ip);
    std::string stringify(std::span<const std::byte, 16> ip);

    tl::expected<std::vector<Interface>, std::error_code> interfaces();
}

template<typename Char>
struct fmt::formatter<zero::os::net::IfAddress4, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    auto format(const zero::os::net::IfAddress4 &address, FmtContext &ctx) const {
        int mask = 0;

        for (auto b: address.mask) {
            while (std::to_integer<int>(b)) {
                if (std::to_integer<int>(b & std::byte{1}))
                    mask++;

                b >>= 1;
            }
        }

        return fmt::format_to(ctx.out(), "{}/{}", zero::os::net::stringify(address.ip), std::to_string(mask));
    }
};

template<typename Char>
struct fmt::formatter<zero::os::net::IfAddress6, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::os::net::IfAddress6 &address, FmtContext &ctx) {
        return std::ranges::copy(zero::os::net::stringify(address.ip), ctx.out()).out;
    }
};

#endif //ZERO_NET_H
