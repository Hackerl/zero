#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <array>
#include <string>
#include <vector>
#include <variant>
#include <system_error>
#include <tl/expected.hpp>
#include <nonstd/span.hpp>

namespace zero::os::net {
    struct IPv4Address {
        std::array<std::byte, 4> ip;
        std::array<std::byte, 4> mask;
    };

    struct IPv6Address {
        std::array<std::byte, 16> ip;
    };

    using Address = std::variant<IPv4Address, IPv6Address>;

    struct Interface {
        std::string name;
        std::array<std::byte, 6> mac;
        std::vector<Address> addresses;
    };

    std::string stringify(nonstd::span<const std::byte, 4> ip);
    std::string stringify(nonstd::span<const std::byte, 16> ip);

    std::string stringify(const IPv4Address &address);
    std::string stringify(const IPv6Address &address);
    std::string stringify(const Address &address);

    tl::expected<std::vector<Interface>, std::error_code> interfaces();
}

#endif //ZERO_NET_H
