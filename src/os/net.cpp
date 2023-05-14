#include <zero/os/net.h>
#include <zero/strings/strings.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <memory>
#include <ws2tcpip.h>
#elif __linux__
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <algorithm>
#include <map>
#if __ANDROID__ && __ANDROID_API__ < 24
#include <dlfcn.h>
#endif
#endif

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

        std::vector<IPAddress> addresses;

        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
            switch (addr->Address.lpSockaddr->sa_family) {
                case AF_INET: {
                    char address[16] = {};

                    if (!inet_ntop(AF_INET, &((sockaddr_in *) addr->Address.lpSockaddr)->sin_addr, address, sizeof(address)))
                        break;

                    addresses.push_back({IPV4, address});
                    break;
                }

                case AF_INET6: {
                    char address[46] = {};

                    if (!inet_ntop(AF_INET6, &((sockaddr_in6 *) addr->Address.lpSockaddr)->sin6_addr, address, sizeof(address)))
                        break;

                    addresses.push_back({IPV6, address});
                    break;
                }

                default:
                    break;
            }
        }

        if (adapter->PhysicalAddressLength != 6)
            continue;

        interfaces.push_back(
                {
                        *name,
                        strings::format(
                                "%02x:%02x:%02x:%02x:%02x:%02x",
                                adapter->PhysicalAddress[0],
                                adapter->PhysicalAddress[1],
                                adapter->PhysicalAddress[2],
                                adapter->PhysicalAddress[3],
                                adapter->PhysicalAddress[4],
                                adapter->PhysicalAddress[5]
                        ),
                        addresses
                }
        );
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
                char address[INET_ADDRSTRLEN] = {};

                if (!inet_ntop(AF_INET, &((sockaddr_in *) p->ifa_addr)->sin_addr, address, sizeof(address)))
                    break;

                interfaceTable[p->ifa_name].addresses.push_back({IPV4, address});
                break;
            }

            case AF_INET6: {
                char address[INET6_ADDRSTRLEN] = {};

                if (!inet_ntop(AF_INET6, &((sockaddr_in6 *) p->ifa_addr)->sin6_addr, address, sizeof(address)))
                    break;

                interfaceTable[p->ifa_name].addresses.push_back({IPV6, address});
                break;
            }

            case AF_PACKET: {
                auto address = (sockaddr_ll *) p->ifa_addr;

                if (address->sll_halen != 6)
                    break;

                interfaceTable[p->ifa_name].mac = strings::format(
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                        address->sll_addr[0],
                        address->sll_addr[1],
                        address->sll_addr[2],
                        address->sll_addr[3],
                        address->sll_addr[4],
                        address->sll_addr[5]
                );

                break;
            }

            default:
                break;
        }
    }

    freeifaddrs(addr);

    std::vector<Interface> interfaces;

    std::transform(interfaceTable.begin(), interfaceTable.end(), std::back_inserter(interfaces), [](const auto &it) {
        return it.second;
    });

    return interfaces;
#endif
}
