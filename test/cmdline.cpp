#include <zero/cmdline.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

struct Config {
    std::string username;
    std::string password;
};

template<>
tl::expected<Config, std::error_code> zero::scan(const std::string_view input) {
    const auto tokens = strings::split(input, ":");

    if (tokens.size() != 2)
        return tl::unexpected(make_error_code(std::errc::invalid_argument));

    return Config{strings::trim(tokens[0]), strings::trim(tokens[1])};
}

TEST_CASE("parse command line arguments", "[cmdline]") {
    constexpr std::array argv = {
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
    REQUIRE(cmdline.get<std::vector<short>>("ports") == std::vector<short>{8080, 8090, 9090});
    REQUIRE(*cmdline.getOptional<std::filesystem::path>("output") == "/tmp/out");
    REQUIRE(!cmdline.getOptional<std::string>("decompress"));
    REQUIRE(*cmdline.getOptional<int>("count") == 6);

    const auto config = cmdline.getOptional<Config>("config");

    REQUIRE(config);
    REQUIRE(config->username == "root");
    REQUIRE(config->password == "123456");
}