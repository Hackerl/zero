#include <zero/cmdline.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

struct Config {
    std::string username;
    std::string password;
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

    const auto output = cmdline.getOptional<std::filesystem::path>("output");
    REQUIRE(output);
    REQUIRE(*output == "/tmp/out");

    REQUIRE_FALSE(cmdline.getOptional<std::string>("decompress"));

    const auto count = cmdline.getOptional<int>("count");
    REQUIRE(count);
    REQUIRE(*count == 6);

    const auto config = cmdline.getOptional<Config>("config");
    REQUIRE(config);
    REQUIRE(config->username == "root");
    REQUIRE(config->password == "123456");
}
