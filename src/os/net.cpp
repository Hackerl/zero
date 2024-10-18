#include <zero/os/net.h>

#ifdef _WIN32
#include <memory>
#include <cassert>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <zero/expect.h>
#include <zero/strings/strings.h>
#elif defined(__linux__)
#include <cstring>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>
#ifdef __ANDROID__
#include <unistd.h>
#if __ANDROID_API__ < 24
#include <dlfcn.h>
#endif
#endif
#elif defined(__APPLE__)
#include <cstring>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <zero/defer.h>
#include <zero/expect.h>
#include <zero/os/unix/error.h>
#endif

std::string zero::os::net::stringify(const std::span<const std::byte, 4> ip) {
    std::array<char, INET_ADDRSTRLEN> address{};

    if (!inet_ntop(AF_INET, ip.data(), address.data(), address.size()))
        throw std::runtime_error("unable to convert IPv4 address from binary to text form");

    return address.data();
}

std::string zero::os::net::stringify(const std::span<const std::byte, 16> ip) {
    std::array<char, INET6_ADDRSTRLEN> address{};

    if (!inet_ntop(AF_INET6, ip.data(), address.data(), address.size()))
        throw std::runtime_error("unable to convert IPv6 address from binary to text form");

    return address.data();
}

std::expected<std::map<std::string, zero::os::net::Interface>, std::error_code> zero::os::net::interfaces() {
#ifdef _WIN32
    ULONG size{10240};
    auto buffer = std::make_unique<std::byte[]>(size);

    while (true) {
        const auto result = GetAdaptersAddresses(
            AF_UNSPEC,
            0,
            nullptr,
            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get()),
            &size
        );

        if (result == ERROR_SUCCESS)
            break;

        if (result != ERROR_BUFFER_OVERFLOW)
            return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

        buffer = std::make_unique<std::byte[]>(size);
    }

    std::map<std::string, Interface> interfaces;

    for (auto adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(buffer.get()); adapter;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        std::array<WCHAR, NDIS_IF_MAX_STRING_SIZE + 1> buf{};

        if (ConvertInterfaceLuidToNameW(&adapter->Luid, buf.data(), buf.size()) != ERROR_SUCCESS)
            return std::unexpected{std::error_code{static_cast<int>(GetLastError()), std::system_category()}};

        const auto name = strings::encode(buf.data());
        EXPECT(name);

        std::vector<Address> addresses;

        for (const auto *addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
            switch (addr->Address.lpSockaddr->sa_family) {
            case AF_INET: {
                IfAddress4 address{};

                std::memcpy(
                    address.ip.data(),
                    &reinterpret_cast<const sockaddr_in *>(addr->Address.lpSockaddr)->sin_addr,
                    4
                );

                address.prefix = addr->OnLinkPrefixLength;
                addresses.emplace_back(address);
                break;
            }

            case AF_INET6: {
                IfAddress6 address{};

                std::memcpy(
                    address.ip.data(),
                    &reinterpret_cast<const sockaddr_in6 *>(addr->Address.lpSockaddr)->sin6_addr,
                    16
                );

                address.prefix = addr->OnLinkPrefixLength;
                addresses.emplace_back(address);
                break;
            }

            default:
                break;
            }
        }

        interfaces.emplace(
            *name,
            Interface{
                *name,
                {
                    reinterpret_cast<const std::byte *>(adapter->PhysicalAddress),
                    reinterpret_cast<const std::byte *>(adapter->PhysicalAddress) + adapter->PhysicalAddressLength
                },
                std::move(addresses)
            }
        );
    }

    return interfaces;
#elif defined(__linux__) || __APPLE__
#if defined(__ANDROID__) && __ANDROID_API__ < 24
    static const auto getifaddrs = reinterpret_cast<int (*)(ifaddrs **)>(dlsym(RTLD_DEFAULT, "getifaddrs"));
    static const auto freeifaddrs = reinterpret_cast<void (*)(ifaddrs *)>(dlsym(RTLD_DEFAULT, "freeifaddrs"));

    if (!getifaddrs || !freeifaddrs)
        return std::unexpected{GetInterfacesError::API_NOT_AVAILABLE};
#endif
    ifaddrs *addr{};

    EXPECT(unix::expected([&] {
        return getifaddrs(&addr);
    }));
    DEFER(freeifaddrs(addr));

    std::map<std::string, Interface> interfaces;

    for (const auto *p = addr; p; p = p->ifa_next) {
        if (!(p->ifa_flags & IFF_UP && p->ifa_flags & IFF_RUNNING))
            continue;

        if (p->ifa_flags & IFF_LOOPBACK)
            continue;

        if (!p->ifa_addr)
            continue;

        auto &[name, mac, addresses] = interfaces[p->ifa_name];
        name = p->ifa_name;

        switch (p->ifa_addr->sa_family) {
        case AF_INET: {
            IfAddress4 address{};

            std::memcpy(address.ip.data(), &reinterpret_cast<const sockaddr_in *>(p->ifa_addr)->sin_addr, 4);

            IPv4 mask{};
            std::memcpy(mask.data(), &reinterpret_cast<const sockaddr_in *>(p->ifa_netmask)->sin_addr, 4);

            int prefix{0};

            for (auto b: mask) {
                while (std::to_integer<int>(b)) {
                    if (std::to_integer<int>(b & std::byte{1}))
                        ++prefix;

                    b >>= 1;
                }
            }

            address.prefix = prefix;
            addresses.emplace_back(address);
            break;
        }

        case AF_INET6: {
            IfAddress6 address{};
            std::memcpy(address.ip.data(), &reinterpret_cast<const sockaddr_in6 *>(p->ifa_addr)->sin6_addr, 16);

            IPv6 mask{};
            std::memcpy(mask.data(), &reinterpret_cast<const sockaddr_in6 *>(p->ifa_netmask)->sin6_addr, 16);

            int prefix{0};

            for (auto b: mask) {
                while (std::to_integer<int>(b)) {
                    if (std::to_integer<int>(b & std::byte{1}))
                        ++prefix;

                    b >>= 1;
                }
            }

            address.prefix = prefix;
            addresses.emplace_back(address);
            break;
        }

#ifdef __APPLE__
        case AF_LINK: {
            const auto address = reinterpret_cast<const sockaddr_dl *>(p->ifa_addr);

            mac.assign(
                reinterpret_cast<const std::byte *>(LLADDR(address)),
                reinterpret_cast<const std::byte *>(LLADDR(address)) + address->sdl_alen
            );

            break;
        }
#else
        case AF_PACKET: {
            const auto address = reinterpret_cast<const sockaddr_ll *>(p->ifa_addr);

            mac.assign(
                reinterpret_cast<const std::byte *>(address->sll_addr),
                reinterpret_cast<const std::byte *>(address->sll_addr) + address->sll_halen
            );

            break;
        }
#endif

        default:
            break;
        }
    }

#ifdef __ANDROID__
    const auto fd = unix::expected([&] {
        return socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    });
    EXPECT(fd);
    DEFER(close(*fd));

    for (auto &[name, mac, addresses]: std::views::values(interfaces)) {
        ifreq request{};

        request.ifr_addr.sa_family = AF_INET;
        std::strncpy(request.ifr_name, name.c_str(), IFNAMSIZ);

        EXPECT(unix::expected([&] {
            return ioctl(*fd, SIOCGIFHWADDR, &request);
        }));

        mac.assign(
            reinterpret_cast<const std::byte *>(request.ifr_hwaddr.sa_data),
            reinterpret_cast<const std::byte *>(request.ifr_hwaddr.sa_data) + 6
        );
    }
#endif

    return interfaces;
#else
#error "unsupported platform"
#endif
}

#if defined(__ANDROID__) && __ANDROID_API__ < 24
DEFINE_ERROR_CATEGORY_INSTANCE(zero::os::net::GetInterfacesError)
#endif
