#include <zero/os/net.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <zero/strings/strings.h>
#endif

#include <zero/os/process.h>

TEST_CASE("network", "[os]") {
    SECTION("basic components") {
        REQUIRE(zero::os::net::stringify(zero::os::net::UNSPECIFIED_IPV4) == "0.0.0.0");
        REQUIRE(zero::os::net::stringify(zero::os::net::LOCALHOST_IPV4) == "127.0.0.1");
        REQUIRE(zero::os::net::stringify(zero::os::net::BROADCAST_IPV4) == "255.255.255.255");
        REQUIRE(zero::os::net::stringify(zero::os::net::UNSPECIFIED_IPV6) == "::");
        REQUIRE(zero::os::net::stringify(zero::os::net::LOCALHOST_IPV6) == "::1");

        REQUIRE(fmt::to_string(zero::os::net::IfAddress4{zero::os::net::LOCALHOST_IPV4, 8}) == "127.0.0.1/8");
        REQUIRE(fmt::to_string(zero::os::net::IfAddress6{zero::os::net::LOCALHOST_IPV6, 128}) == "::1/128");

        REQUIRE(
            fmt::to_string(
                zero::os::net::Interface{
                    "lo",
                    {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}},
                    {
                        zero::os::net::IfAddress4{zero::os::net::LOCALHOST_IPV4, 8},
                        zero::os::net::IfAddress6{zero::os::net::LOCALHOST_IPV6, 128}
                    }
                }
            ) == R"({ name: "lo", mac: "00:00:00:00:00:00", addresses: ["127.0.0.1/8", "::1/128"] })"
        );
    }

    SECTION("interfaces") {
        const auto interfaces = zero::os::net::interfaces();
        REQUIRE(interfaces);

#ifdef _WIN32
        const auto output = zero::os::process::Command("ipconfig").arg("/all").output();
#elif defined(__linux__)
        const auto output = zero::os::process::Command("ip").arg("a").output();
#else
        const auto output = zero::os::process::Command("ifconfig").output();
#endif
        REQUIRE(output);
        REQUIRE(output->status.success());

#ifdef _WIN32
        const auto str = zero::strings::decode(
            {reinterpret_cast<const char *>(output->out.data()), output->out.size()},
            fmt::format("CP{}", GetACP())
        ).and_then([](const auto &s) {
            return zero::strings::encode(s);
        });
        REQUIRE(str);
        const auto &result = *str;
#else
        const std::string result{reinterpret_cast<const char *>(output->out.data()), output->out.size()};
#endif

        for (const auto &[name, mac, addresses]: ranges::views::values(*interfaces)) {
#ifdef _WIN32
            NET_LUID id{};
            REQUIRE(ConvertInterfaceNameToLuidA(name.c_str(), &id) == ERROR_SUCCESS);

            std::array<WCHAR, NDIS_IF_MAX_STRING_SIZE + 1> buffer{};
            REQUIRE(ConvertInterfaceLuidToAlias(&id, buffer.data(), buffer.size()) == ERROR_SUCCESS);

            const auto alias = zero::strings::encode(buffer.data());
            REQUIRE(alias);

            REQUIRE_THAT(result, Catch::Matchers::ContainsSubstring(fmt::format("{}:", *alias)));
            REQUIRE_THAT(
                result,
                Catch::Matchers::ContainsSubstring(to_string(
                    fmt::join(
                        mac | ranges::views::transform([](const auto byte) {
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
                        mac | ranges::views::transform([](const auto byte) {
                            return fmt::format("{:02x}", byte);
                        }),
                        ":"
                    )
                ))
            );
#endif

            REQUIRE_THAT(
                addresses
                | ranges::views::transform([](const auto &address) {
                    if (std::holds_alternative<zero::os::net::IfAddress4>(address))
                        return zero::os::net::stringify(std::get<zero::os::net::IfAddress4>(address).ip);

                    return zero::os::net::stringify(std::get<zero::os::net::IfAddress6>(address).ip);
                })
                | ranges::to<std::list>(),
                Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::string>([&](const auto &ip) {
                    return result.find(ip) != std::string::npos;
                }))
            );
        }
    }
}
