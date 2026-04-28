#ifndef ZERO_OS_NET_H
#define ZERO_OS_NET_H

#include <map>
#include <span>
#include <array>
#include <ranges>
#include <fmt/std.h>
#include <fmt/ranges.h>

namespace zero::os::net {
    using IPv4 = std::array<std::byte, 4>;
    using IPv6 = std::array<std::byte, 16>;
    using IP = std::variant<IPv4, IPv6>;

    inline constexpr IPv4 LocalhostIPv4 = {std::byte{127}, std::byte{0}, std::byte{0}, std::byte{1}};
    inline constexpr IPv4 BroadcastIPv4 = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    inline constexpr IPv4 UnspecifiedIPv4 = {};

    inline constexpr IPv6 LocalhostIPv6 = {
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}
    };

    inline constexpr IPv6 UnspecifiedIPv6 = {};

    struct IfAddress4 {
        IPv4 ip{};
        int prefix{};
    };

    struct IfAddress6 {
        IPv6 ip{};
        int prefix{};
    };

    using Address = std::variant<IfAddress4, IfAddress6>;

    struct Interface {
        std::string name;
        std::vector<std::byte> mac;
        std::vector<Address> addresses;
    };

    std::string stringify(std::span<const std::byte, 4> ip);
    std::string stringify(std::span<const std::byte, 16> ip);

    std::map<std::string, Interface> interfaces();
}

template<typename Char>
struct fmt::formatter<zero::os::net::IfAddress4, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::os::net::IfAddress4 &address, FmtContext &ctx) {
        return fmt::format_to(ctx.out(), "{}/{}", zero::os::net::stringify(address.ip), address.prefix);
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
        return fmt::format_to(ctx.out(), "{}/{}", zero::os::net::stringify(address.ip), address.prefix);
    }
};

template<typename Char>
struct fmt::formatter<zero::os::net::Interface, Char> {
    template<typename ParseContext>
    static constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template<typename FmtContext>
    static auto format(const zero::os::net::Interface &i, FmtContext &ctx) {
        return fmt::format_to(
            ctx.out(),
            R"({{ name: {:?}, mac: "{}", addresses: {} }})",
            i.name,
            fmt::join(
                i.mac | std::views::transform([](const auto byte) {
                    return fmt::format("{:02x}", byte);
                }),
                ":"
            ),
            i.addresses | std::views::transform([](const auto &address) {
                if (const auto addr = std::get_if<zero::os::net::IfAddress4>(&address))
                    return fmt::to_string(*addr);

                return fmt::to_string(std::get<zero::os::net::IfAddress6>(address));
            })
        );
    }
};

#endif //ZERO_OS_NET_H
