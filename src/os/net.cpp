#include <zero/os/net.h>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <memory>
#include <ws2tcpip.h>
#include <zero/strings/strings.h>
#elif __linux__
#include <map>
#include <cstring>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <zero/defer.h>
#if __ANDROID__
#include <unistd.h>
#include <linux/if.h>
#if __ANDROID_API__ < 24
#include <dlfcn.h>
#endif
#endif
#elif __APPLE__
#include <map>
#include <cstring>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <zero/defer.h>
#endif

std::string zero::os::net::stringify(const nonstd::span<const std::byte, 4> ip) {
    char address[INET_ADDRSTRLEN] = {};

    if (!inet_ntop(AF_INET, ip.data(), address, sizeof(address)))
        throw std::runtime_error("unable to convert IPv4 address from binary to text form");

    return address;
}

std::string zero::os::net::stringify(const nonstd::span<const std::byte, 16> ip) {
    char address[INET6_ADDRSTRLEN] = {};

    if (!inet_ntop(AF_INET6, ip.data(), address, sizeof(address)))
        throw std::runtime_error("unable to convert IPv6 address from binary to text form");

    return address;
}

tl::expected<std::vector<zero::os::net::Interface>, std::error_code> zero::os::net::interfaces() {
#ifdef _WIN32
    ULONG length = 0;

    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &length) != ERROR_BUFFER_OVERFLOW)
        return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

    if (length == 0)
        return {};

    const auto buffer = std::make_unique<char[]>(length);

    if (GetAdaptersAddresses(
        AF_UNSPEC,
        0,
        nullptr,
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get()),
        &length
    ) != ERROR_SUCCESS)
        return tl::unexpected<std::error_code>(static_cast<int>(GetLastError()), std::system_category());

    std::vector<Interface> interfaces;

    for (auto adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(buffer.get()); adapter;
         adapter = adapter->Next) {
        auto name = strings::encode(adapter->FriendlyName);

        if (!name)
            continue;

        std::vector<Address> addresses;

        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
            switch (addr->Address.lpSockaddr->sa_family) {
            case AF_INET: {
                IfAddress4 address = {};

                memcpy(address.ip.data(), &reinterpret_cast<sockaddr_in *>(addr->Address.lpSockaddr)->sin_addr, 4);

                if (ConvertLengthToIpv4Mask(
                    addr->OnLinkPrefixLength,
                    reinterpret_cast<PULONG>(address.mask.data())
                ) != NO_ERROR)
                    break;

                addresses.emplace_back(address);
                break;
            }

            case AF_INET6: {
                IfAddress6 address = {};

                memcpy(
                    address.ip.data(),
                    &reinterpret_cast<sockaddr_in6 *>(addr->Address.lpSockaddr)->sin6_addr,
                    16
                );

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
        memcpy(item.mac.data(), adapter->PhysicalAddress, 6);

        interfaces.push_back(std::move(item));
    }

    return interfaces;
#elif __linux__ || __APPLE__
#if __ANDROID__ && __ANDROID_API__ < 24
    static const auto getifaddrs = reinterpret_cast<int (*)(ifaddrs **)>(dlsym(RTLD_DEFAULT, "getifaddrs"));
    static const auto freeifaddrs = reinterpret_cast<void (*)(ifaddrs *)>(dlsym(RTLD_DEFAULT, "freeifaddrs"));

    if (!getifaddrs || !freeifaddrs)
        return tl::unexpected(make_error_code(std::errc::function_not_supported));
#endif
    ifaddrs *addr;

    if (getifaddrs(&addr) < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    DEFER(freeifaddrs(addr));
    std::map<std::string, Interface> interfaceTable;

    for (const ifaddrs *p = addr; p; p = p->ifa_next) {
        if (!p->ifa_addr)
            continue;

        auto &[name, mac, addresses] = interfaceTable[p->ifa_name];
        name = p->ifa_name;

        switch (p->ifa_addr->sa_family) {
        case AF_INET: {
            IfAddress4 address = {};

            memcpy(address.ip.data(), &reinterpret_cast<sockaddr_in *>(p->ifa_addr)->sin_addr, 4);
            memcpy(address.mask.data(), &reinterpret_cast<sockaddr_in *>(p->ifa_netmask)->sin_addr, 4);

            addresses.emplace_back(address);
            break;
        }

        case AF_INET6: {
            IfAddress6 address = {};
            memcpy(address.ip.data(), &reinterpret_cast<sockaddr_in6 *>(p->ifa_addr)->sin6_addr, 16);

            addresses.emplace_back(address);
            break;
        }

#ifdef __APPLE__
        case AF_LINK: {
            const auto address = reinterpret_cast<const sockaddr_dl *>(p->ifa_addr);

            if (address->sdl_alen != 6)
                break;

            memcpy(mac.data(), LLADDR(address), 6);
            break;
        }
#else
        case AF_PACKET: {
            const auto address = reinterpret_cast<const sockaddr_ll *>(p->ifa_addr);

            if (address->sll_halen != 6)
                break;

            memcpy(mac.data(), address->sll_addr, 6);
            break;
        }
#endif

        default:
            break;
        }
    }

    auto v = ranges::views::values(interfaceTable);

#ifdef __ANDROID__
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (fd < 0)
        return tl::unexpected<std::error_code>(errno, std::system_category());

    DEFER(close(fd));

    for (auto &[name, mac, addresses]: v) {
        ifreq request = {};

        request.ifr_addr.sa_family = AF_INET;
        strncpy(request.ifr_name, name.c_str(), IFNAMSIZ);

        if (ioctl(fd, SIOCGIFHWADDR, &request) < 0)
            return tl::unexpected<std::error_code>(errno, std::system_category());

        memcpy(mac.data(), request.ifr_hwaddr.sa_data, 6);
    }
#endif

    return std::vector(v.begin(), v.end());
#else
#error "unsupported platform"
#endif
}
