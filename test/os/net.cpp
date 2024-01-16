#include <zero/os/net.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("stringify IP address", "[os]") {
    std::array<std::byte, 4> ipv4 = {};
    REQUIRE(zero::os::net::stringify(ipv4) == "0.0.0.0");

    ipv4 = {std::byte{127}, std::byte{0}, std::byte{0}, std::byte{1}};
    REQUIRE(zero::os::net::stringify(ipv4) == "127.0.0.1");

    ipv4 = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    REQUIRE(zero::os::net::stringify(ipv4) == "255.255.255.255");

    REQUIRE(zero::os::net::stringify(std::array<std::byte, 16>{}) == "::");

    constexpr std::array ipv6 = {
        std::byte{253}, std::byte{189}, std::byte{220}, std::byte{2},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{1},
        std::byte{0}, std::byte{9}, std::byte{0}, std::byte{0},
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{141}
    };

    REQUIRE(zero::os::net::stringify(ipv6) == "fdbd:dc02:ff:1:9::8d");

    ipv4 = {std::byte{192}, std::byte{168}, std::byte{10}, std::byte{1}};
    std::array mask = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

    REQUIRE(fmt::to_string(zero::os::net::IfAddress4{ipv4, mask}) == "192.168.10.1/32");

    mask = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{0}};
    REQUIRE(fmt::to_string(zero::os::net::IfAddress4{ipv4, mask}) == "192.168.10.1/24");

    mask = {std::byte{255}, std::byte{255}, std::byte{240}, std::byte{0}};
    REQUIRE(fmt::to_string(zero::os::net::IfAddress4{ipv4, mask}) == "192.168.10.1/20");

    mask = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    REQUIRE(fmt::to_string(zero::os::net::IfAddress4{ipv4, mask}) == "192.168.10.1/0");

    zero::os::net::Address address = zero::os::net::IfAddress4{ipv4, mask};
    REQUIRE(fmt::to_string(address) == "variant(192.168.10.1/0)");

    address = zero::os::net::IfAddress6{ipv6};
    REQUIRE(fmt::to_string(address) == "variant(fdbd:dc02:ff:1:9::8d)");

    zero::os::net::Interface interface = {
        "lo",
        {},
        {zero::os::net::IfAddress4{ipv4, mask}, zero::os::net::IfAddress6{ipv6}}
    };

    REQUIRE(
        fmt::to_string(interface) ==
        R"({ name: "lo", mac: "00:00:00:00:00:00", addresses: ["192.168.10.1/0", "fdbd:dc02:ff:1:9::8d"] })"
    );
}

TEST_CASE("fetch network interface", "[os]") {
    REQUIRE(zero::os::net::interfaces());
}
