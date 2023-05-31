#include <zero/cmdline.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

struct Config {
    std::string username;
    std::string password;
};

template<>
std::optional<Config> zero::convert<Config>(std::string_view str) {
    std::vector<std::string> tokens = zero::strings::split(str, ":");

    if (tokens.size() != 2)
        return std::nullopt;

    return Config{zero::strings::trim(tokens[0]), zero::strings::trim(tokens[1])};
}

TEST_CASE("parse command line arguments", "[cmdline]") {
    std::array<const char *, 8> argv = {
            "cmdline",
            "--output=/tmp/out",
            "-c",
            "6",
            "--http",
            "--config=root:123456",
            "localhost",
            "8080"
    };

    zero::Cmdline cmdline;

    cmdline.add<std::string>("host", "remote host");
    cmdline.add<short>("port", "remote port");

    cmdline.addOptional("http", '\0', "http protocol");
    cmdline.addOptional<std::filesystem::path>("output", '\0', "output path");
    cmdline.addOptional<std::string>("decompress", '\0', "decompress method");
    cmdline.addOptional<int>("count", 'c', "thread count");
    cmdline.addOptional<Config>("config", '\0', "account config");

    cmdline.parse(argv.size(), argv.data());

    REQUIRE(cmdline.exist("http"));
    REQUIRE(cmdline.get<std::string>("host") == "localhost");
    REQUIRE(cmdline.get<short>("port") == 8080);
    REQUIRE(*cmdline.getOptional<std::filesystem::path>("output") == "/tmp/out");
    REQUIRE(!cmdline.getOptional<std::string>("decompress"));
    REQUIRE(*cmdline.getOptional<int>("count") == 6);

    std::optional<Config> config = cmdline.getOptional<Config>("config");

    REQUIRE(config);
    REQUIRE(config->username == "root");
    REQUIRE(config->password == "123456");
}