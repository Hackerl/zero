#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <nonstd/span.hpp>

namespace zero::os::net {
    struct IPv4Address {
        std::byte address[4];
        std::byte mask[4];
    };

    struct IPv6Address {
        std::byte address[16];
    };

    struct Interface {
        std::string name;
        std::byte mac[6];
        std::vector<std::variant<IPv4Address, IPv6Address>> addresses;
    };

    std::string stringify(nonstd::span<const std::byte, 4> ip);
    std::string stringify(nonstd::span<const std::byte, 16> ip);

    std::optional<std::vector<Interface>> interfaces();
}

#endif //ZERO_NET_H
