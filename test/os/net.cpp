#include <catch_extensions.h>
#include <zero/os/net.h>
#include <catch2/matchers/catch_matchers_all.hpp>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <zero/strings.h>
#endif

#include <zero/os/process.h>

TEST_CASE("network components", "[os::net]") {
    REQUIRE(zero::os::net::stringify(zero::os::net::UnspecifiedIPv4) == "0.0.0.0");
    REQUIRE(zero::os::net::stringify(zero::os::net::LocalhostIPv4) == "127.0.0.1");
    REQUIRE(zero::os::net::stringify(zero::os::net::BroadcastIPv4) == "255.255.255.255");
    REQUIRE(zero::os::net::stringify(zero::os::net::UnspecifiedIPv6) == "::");
    REQUIRE(zero::os::net::stringify(zero::os::net::LocalhostIPv6) == "::1");

    REQUIRE(fmt::to_string(zero::os::net::IfAddress4{zero::os::net::LocalhostIPv4, 8}) == "127.0.0.1/8");
    REQUIRE(fmt::to_string(zero::os::net::IfAddress6{zero::os::net::LocalhostIPv6, 128}) == "::1/128");

    REQUIRE(
        fmt::to_string(
            zero::os::net::Interface{
                "lo",
                {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}},
                {
                    zero::os::net::IfAddress4{zero::os::net::LocalhostIPv4, 8},
                    zero::os::net::IfAddress6{zero::os::net::LocalhostIPv6, 128}
                }
            }
        ) == R"({ name: "lo", mac: "00:00:00:00:00:00", addresses: ["127.0.0.1/8", "::1/128"] })"
    );
}

#ifndef ZERO_PROCESS_PARTIAL_API
TEST_CASE("list network interfaces", "[os::net]") {
    const auto interfaces = zero::os::net::interfaces();

#ifdef _WIN32
    const auto output = zero::error::guard(zero::os::process::Command{"ipconfig"}.arg("/all").output());
#elif defined(__linux__)
    const auto output = zero::error::guard(zero::os::process::Command{"ip"}.arg("a").output());
#else
    const auto output = zero::error::guard(zero::os::process::Command{"ifconfig"}.output());
#endif
    REQUIRE(output.status.success());

#ifdef _WIN32
    const auto result = zero::error::guard(
        zero::strings::decode(
            {reinterpret_cast<const char *>(output.out.data()), output.out.size()},
            fmt::format("CP{}", GetACP())
        ).and_then([](const auto &s) {
            return zero::strings::encode(s);
        })
    );
#else
    const std::string result{reinterpret_cast<const char *>(output.out.data()), output.out.size()};
#endif

    for (const auto &[name, mac, addresses]: interfaces | std::views::values) {
#ifdef _WIN32
        NET_LUID id{};
        REQUIRE(ConvertInterfaceNameToLuidA(name.c_str(), &id) == ERROR_SUCCESS);

        std::array<WCHAR, NDIS_IF_MAX_STRING_SIZE + 1> buffer{};
        REQUIRE(ConvertInterfaceLuidToAlias(&id, buffer.data(), buffer.size()) == ERROR_SUCCESS);

        const auto alias = zero::error::guard(zero::strings::encode(buffer.data()));

        REQUIRE_THAT(result, Catch::Matchers::ContainsSubstring(fmt::format("{}:", alias)));
        REQUIRE_THAT(
            result,
            Catch::Matchers::ContainsSubstring(to_string(
                fmt::join(
                    mac | std::views::transform([](const auto byte) {
                        return fmt::format("{:02X}", byte);
                    }),
                    "-"
                )
            ))
        );
#else
        REQUIRE_THAT(result, Catch::Matchers::ContainsSubstring(fmt::format("{}:", name)));
        REQUIRE_THAT(
            result,
            Catch::Matchers::ContainsSubstring(to_string(
                fmt::join(
                    mac | std::views::transform([](const auto byte) {
                        return fmt::format("{:02x}", byte);
                    }),
                    ":"
                )
            ))
        );
#endif

        REQUIRE_THAT(
            addresses
            | std::views::transform([](const auto &address) {
                if (const auto addr = std::get_if<zero::os::net::IfAddress4>(&address))
                    return zero::os::net::stringify(addr->ip);

                return zero::os::net::stringify(std::get<zero::os::net::IfAddress6>(address).ip);
            })
            | std::ranges::to<std::list>(),
            Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::string>([&](const auto &ip) {
                return result.contains(ip);
            }))
        );
    }
}
#endif
