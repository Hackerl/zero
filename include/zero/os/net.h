#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <span>
#include <array>
#include <string>
#include <vector>
#include <ranges>
#include <variant>
#include <system_error>
#include <tl/expected.hpp>
#include <fmt/std.h>
#include <fmt/ranges.h>

#if __ANDROID__ && __ANDROID_API__ < 24
#include <zero/error.h>
#endif

namespace zero::os::net {
#if __ANDROID__ && __ANDROID_API__ < 24
    DEFINE_ERROR_CODE_EX(
        GetInterfacesError,
        "zero::os::net::interfaces",
        API_NOT_AVAILABLE, "api not available", std::errc::function_not_supported
    )
#endif
    using MAC = std::array<std::byte, 6>;
    using IPv4 = std::array<std::byte, 4>;
    using IPv6 = std::array<std::byte, 16>;
    using IP = std::variant<IPv4, IPv6>;

    struct IfAddress4 {
        IPv4 ip;
        IPv4 mask;
    };

    struct IfAddress6 {
        IPv6 ip;
    };

    using Address = std::variant<IfAddress4, IfAddress6>;

    struct Interface {
        std::string name;
        MAC mac;
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
    static auto format(const zero::os::net::IfAddress4 &address, FmtContext &ctx) {
        int mask = 0;

        for (auto b: address.mask) {
            while (std::to_integer<int>(b)) {
                if (std::to_integer<int>(b & std::byte{1}))
                    ++mask;

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
            R"({{ name: {:?}, mac: "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", addresses: {} }})",
            i.name,
            i.mac[0], i.mac[1], i.mac[2], i.mac[3], i.mac[4], i.mac[5],
            i.addresses | std::views::transform([](const auto &address) {
                if (std::holds_alternative<zero::os::net::IfAddress4>(address))
                    return fmt::to_string(std::get<zero::os::net::IfAddress4>(address));

                return fmt::to_string(std::get<zero::os::net::IfAddress6>(address));
            })
        );
    }
};

#if __ANDROID__ && __ANDROID_API__ < 24
DECLARE_ERROR_CODE(zero::os::net::GetInterfacesError)
#endif

#endif //ZERO_NET_H
