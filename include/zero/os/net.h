#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <span>
#include <array>
#include <string>
#include <vector>
#include <variant>
#include <optional>

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

    std::string stringify(std::span<const std::byte, 4> ip);
    std::string stringify(std::span<const std::byte, 16> ip);

    std::string stringify(const IPv4Address &address);
    std::string stringify(const IPv6Address &address);
    std::string stringify(const Address &address);

    std::optional<std::vector<Interface>> interfaces();
}

#endif //ZERO_NET_H
