#ifndef ZERO_NET_H
#define ZERO_NET_H

#include <string>
#include <vector>
#include <optional>

namespace zero::os::net {
    enum AddressType {
        IPV4,
        IPV6
    };

    struct IPAddress {
        AddressType type;
        std::string address;
    };

    struct Interface {
        std::string name;
        std::string mac;
        std::vector<IPAddress> addresses;
    };

    std::optional<std::vector<Interface>> interfaces();
}

#endif //ZERO_NET_H
