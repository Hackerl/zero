#include <zero/os/net.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <memory>
#include <ws2tcpip.h>
#include <zero/strings/strings.h>
#elif __linux__
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <algorithm>
#include <cstring>
#include <map>
#if __ANDROID__
#include <unistd.h>
#include <linux/if.h>
#if __ANDROID_API__ < 24
#include <dlfcn.h>
#endif
#endif
#endif

std::string zero::os::net::stringify(nonstd::span<const std::byte, 4> ip) {
    char address[INET_ADDRSTRLEN] = {};

    if (!inet_ntop(AF_INET, ip.data(), address, sizeof(address)))
        throw std::runtime_error("unable to convert IPv4 address from binary to text form");

    return address;
}

std::string zero::os::net::stringify(nonstd::span<const std::byte, 16> ip) {
    char address[INET6_ADDRSTRLEN] = {};

    if (!inet_ntop(AF_INET6, ip.data(), address, sizeof(address)))
        throw std::runtime_error("unable to convert IPv6 address from binary to text form");

    return address;
}

std::optional<std::vector<zero::os::net::Interface>> zero::os::net::interfaces() {
#ifdef _WIN32
    ULONG length = 0;

    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &length) != ERROR_BUFFER_OVERFLOW)
        return std::nullopt;

    if (length == 0)
        return std::nullopt;

    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length);

    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, (PIP_ADAPTER_ADDRESSES) buffer.get(), &length) != ERROR_SUCCESS)
        return std::nullopt;

    std::vector<Interface> interfaces;

    for (auto adapter = (PIP_ADAPTER_ADDRESSES) buffer.get(); adapter; adapter = adapter->Next) {
        std::optional<std::string> name = zero::strings::encode(adapter->FriendlyName);

        if (!name)
            continue;

        std::vector<std::variant<IPv4Address, IPv6Address>> addresses;

        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
            switch (addr->Address.lpSockaddr->sa_family) {
                case AF_INET: {
                    IPv4Address address = {};

                    memcpy(address.address, &((sockaddr_in *) addr->Address.lpSockaddr)->sin_addr, 4);

                    if (ConvertLengthToIpv4Mask(addr->OnLinkPrefixLength, (PULONG) address.mask) != NO_ERROR)
                        break;

                    addresses.emplace_back(address);
                    break;
                }

                case AF_INET6: {
                    IPv6Address address = {};
                    memcpy(address.address, &((sockaddr_in6 *) addr->Address.lpSockaddr)->sin6_addr, 16);

                    addresses.emplace_back(address);
                    break;
                }

                default:
                    break;
            }
        }

        if (adapter->PhysicalAddressLength != 6)
            continue;

        Interface item;

        item.name = std::move(*name);
        item.addresses = std::move(addresses);

        memcpy(item.mac, adapter->PhysicalAddress, 6);

        interfaces.push_back(std::move(item));
    }

    return interfaces;
#elif __linux__
#if __ANDROID__ && __ANDROID_API__ < 24
    static auto getifaddrs = (int (*)(ifaddrs **)) dlsym(RTLD_DEFAULT, "getifaddrs");
    static auto freeifaddrs = (void (*)(ifaddrs *)) dlsym(RTLD_DEFAULT, "freeifaddrs");

    if (!getifaddrs || !freeifaddrs)
        return std::nullopt;
#endif
    ifaddrs *addr;

    if (getifaddrs(&addr) < 0)
        return std::nullopt;

    std::map<std::string, Interface> interfaceTable;

    for (ifaddrs *p = addr; p; p = p->ifa_next) {
        if (!p->ifa_addr)
            continue;

        interfaceTable[p->ifa_name].name = p->ifa_name;

        switch (p->ifa_addr->sa_family) {
            case AF_INET: {
                IPv4Address address = {};

                memcpy(address.address, &((sockaddr_in *) p->ifa_addr)->sin_addr, 4);
                memcpy(address.mask, &((sockaddr_in *) p->ifa_netmask)->sin_addr, 4);

                interfaceTable[p->ifa_name].addresses.emplace_back(address);
                break;
            }

            case AF_INET6: {
                IPv6Address address = {};
                memcpy(address.address, &((sockaddr_in6 *) p->ifa_addr)->sin6_addr, 16);

                interfaceTable[p->ifa_name].addresses.emplace_back(address);
                break;
            }

            case AF_PACKET: {
                auto address = (sockaddr_ll *) p->ifa_addr;

                if (address->sll_halen != 6)
                    break;

                memcpy(interfaceTable[p->ifa_name].mac, address->sll_addr, 6);
                break;
            }

            default:
                break;
        }
    }

    freeifaddrs(addr);

    std::vector<Interface> interfaces;

    std::transform(
            interfaceTable.begin(),
            interfaceTable.end(),
            std::back_inserter(interfaces),
            [](const auto &it) {
                return it.second;
            }
    );

#if __ANDROID__
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (fd < 0)
        return std::nullopt;

    for (auto &interface: interfaces) {
        ifreq request = {};

        request.ifr_addr.sa_family = AF_INET;
        strncpy(request.ifr_name, interface.name.c_str(), IFNAMSIZ);

        if (ioctl(fd, SIOCGIFHWADDR, &request) < 0)
            continue;

        memcpy(interface.mac, request.ifr_hwaddr.sa_data, 6);
    }

    close(fd);
#endif

    return interfaces;
#endif
}
