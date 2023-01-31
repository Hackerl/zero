#include <zero/cmdline.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

TEST_CASE("parse command line arguments", "[cmdline]") {
    std::array<const char *, 7> argv = {
            "cmdline",
            "--output=/tmp/out",
            "-c",
            "6",
            "--http",
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

    cmdline.parse(argv.size(), argv.data());

    REQUIRE(cmdline.exist("http"));
    REQUIRE(cmdline.get<std::string>("host") == "localhost");
    REQUIRE(cmdline.get<short>("port") == 8080);
    REQUIRE(*cmdline.getOptional<std::filesystem::path>("output") == "/tmp/out");
    REQUIRE(!cmdline.getOptional<std::string>("decompress"));
    REQUIRE(*cmdline.getOptional<int>("count") == 6);
}