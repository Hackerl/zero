#include <zero/os/net.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("stringify IP address", "[net]") {
    std::array<std::byte, 4> ipv4 = {};
    REQUIRE(zero::os::net::stringify(ipv4) == "0.0.0.0");

    ipv4 = {std::byte{127}, std::byte{0}, std::byte{0}, std::byte{1}};
    REQUIRE(zero::os::net::stringify(ipv4) == "127.0.0.1");

    ipv4 = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    REQUIRE(zero::os::net::stringify(ipv4) == "255.255.255.255");

    REQUIRE(zero::os::net::stringify(std::array<std::byte, 16>{}) == "::");

    std::array<std::byte, 16> ipv6 = {
            std::byte{253},
            std::byte{189},
            std::byte{220},
            std::byte{2},
            std::byte{0},
            std::byte{255},
            std::byte{0},
            std::byte{1},
            std::byte{0},
            std::byte{9},
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{141}
    };

    REQUIRE(zero::os::net::stringify(ipv6) == "fdbd:dc02:ff:1:9::8d");

    ipv4 = {std::byte{192}, std::byte{168}, std::byte{10}, std::byte{1}};
    std::array<std::byte, 4> mask = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

    REQUIRE(zero::os::net::stringify(zero::os::net::IPv4Address{ipv4, mask}) == "192.168.10.1/32");

    mask = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{0}};
    REQUIRE(zero::os::net::stringify(zero::os::net::IPv4Address{ipv4, mask}) == "192.168.10.1/24");

    mask = {std::byte{255}, std::byte{255}, std::byte{240}, std::byte{0}};
    REQUIRE(zero::os::net::stringify(zero::os::net::IPv4Address{ipv4, mask}) == "192.168.10.1/20");

    mask = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    REQUIRE(zero::os::net::stringify(zero::os::net::IPv4Address{ipv4, mask}) == "192.168.10.1/0");

    zero::os::net::Address address = zero::os::net::IPv4Address{ipv4, mask};
    REQUIRE(zero::os::net::stringify(address) == "192.168.10.1/0");
}

TEST_CASE("fetch network interface", "[net]") {
    REQUIRE(zero::os::net::interfaces());
}