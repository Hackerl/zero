#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <map>
#include <span>
#include <array>
#include <ranges>
#include <expected>
#include <system_error>
#include <fmt/std.h>
#include <fmt/ranges.h>

#if defined(__ANDROID__) && __ANDROID_API__ < 24
#include <zero/error.h>
#endif

namespace zero::os::net {
    using IPv4 = std::array<std::byte, 4>;
    using IPv6 = std::array<std::byte, 16>;
    using IP = std::variant<IPv4, IPv6>;

    inline constexpr IPv4 LOCALHOST_IPV4 = {std::byte{127}, std::byte{0}, std::byte{0}, std::byte{1}};
    inline constexpr IPv4 BROADCAST_IPV4 = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    inline constexpr IPv4 UNSPECIFIED_IPV4 = {};

    inline constexpr IPv6 LOCALHOST_IPV6 = {
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}
    };

    inline constexpr IPv6 UNSPECIFIED_IPV6 = {};

    struct IfAddress4 {
        IPv4 ip;
        int prefix;
    };

    struct IfAddress6 {
        IPv6 ip;
        int prefix;
    };

    using Address = std::variant<IfAddress4, IfAddress6>;

    struct Interface {
        std::string name;
        std::vector<std::byte> mac;
        std::vector<Address> addresses;
    };

    std::string stringify(std::span<const std::byte, 4> ip);
    std::string stringify(std::span<const std::byte, 16> ip);

#if defined(__ANDROID__) && __ANDROID_API__ < 24
    DEFINE_ERROR_CODE_EX(
        GetInterfacesError,
        "zero::os::net::interfaces",
        API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
    )
#endif

    std::expected<std::map<std::string, Interface>, std::error_code> interfaces();
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
                if (std::holds_alternative<zero::os::net::IfAddress4>(address))
                    return fmt::to_string(std::get<zero::os::net::IfAddress4>(address));

                return fmt::to_string(std::get<zero::os::net::IfAddress6>(address));
            })
        );
    }
};

#if defined(__ANDROID__) && __ANDROID_API__ < 24
DECLARE_ERROR_CODE(zero::os::net::GetInterfacesError)
#endif

#endif //ZERO_NET_H
