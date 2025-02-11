#include "catch_extensions.h"
#include <zero/cmdline.h>
#include <catch2/matchers/catch_matchers_all.hpp>

struct Config {
    std::string username;
    std::string password;

    friend bool operator==(const Config &lhs, const Config &rhs) {
        return lhs.username == rhs.username && lhs.password == rhs.password;
    }

    friend bool operator!=(const Config &lhs, const Config &rhs) {
        return !(lhs == rhs);
    }
};

template<>
tl::expected<Config, std::error_code> zero::scan(const std::string_view input) {
    const auto tokens = strings::split(input, ":", 1);

    if (tokens.size() != 2)
        return tl::unexpected{make_error_code(std::errc::invalid_argument)};

    return Config{strings::trim(tokens[0]), strings::trim(tokens[1])};
}

TEST_CASE("argument parser", "[cmdline]") {
    constexpr std::array argv{
        "cmdline",
        "--output=/tmp/out",
        "-c",
        "6",
        "--http",
        "--config=root:123456",
        "localhost",
        "8080, 8090, 9090",
    };

    zero::Cmdline cmdline;

    cmdline.add<std::string>("host", "remote host");
    cmdline.add<std::vector<short>>("ports", "remote ports");

    cmdline.addOptional("http", '\0', "http protocol");
    cmdline.addOptional<std::filesystem::path>("output", '\0', "output path");
    cmdline.addOptional<std::string>("decompress", '\0', "decompress method");
    cmdline.addOptional<int>("count", 'c', "thread count");
    cmdline.addOptional<Config>("config", '\0', "account config");

    cmdline.footer("footer message");
    cmdline.from(argv.size(), argv.data());

    REQUIRE(cmdline.exist("http"));
    REQUIRE(cmdline.get<std::string>("host") == "localhost");

    REQUIRE_THAT(cmdline.get<std::vector<short>>("ports"), Catch::Matchers::RangeEquals(std::vector{8080, 8090, 9090}));

    REQUIRE(cmdline.getOptional<std::filesystem::path>("output") == "/tmp/out");
    REQUIRE_FALSE(cmdline.getOptional<std::string>("decompress"));
    REQUIRE(cmdline.getOptional<int>("count") == 6);
    REQUIRE(cmdline.getOptional<Config>("config") == Config{"root", "123456"});
}
