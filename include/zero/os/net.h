#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <array>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <nonstd/span.hpp>

namespace zero::os::net {
    struct IPv4Address {
        std::array<std::byte, 4> address;
        std::array<std::byte, 4> mask;
    };

    struct IPv6Address {
        std::array<std::byte, 16> address;
    };

    struct Interface {
        std::string name;
        std::array<std::byte, 6> mac;
        std::vector<std::variant<IPv4Address, IPv6Address>> addresses;
    };

    std::string stringify(nonstd::span<const std::byte, 4> ip);
    std::string stringify(nonstd::span<const std::byte, 16> ip);

    std::optional<std::vector<Interface>> interfaces();
}

#endif //ZERO_NET_H
