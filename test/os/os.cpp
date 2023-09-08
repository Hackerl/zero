#include <zero/os/os.h>
#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>

tl::expected<std::string, std::error_code> exec(const std::string &cmd) {
#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "r");
#else
    FILE *pipe = popen(cmd.c_str(), "r");
#endif

    if (!pipe)
        return tl::unexpected(std::error_code(errno, std::system_category()));

    std::string output;
    char buffer[128];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return output;
}

TEST_CASE("get hostname", "[os]") {
    auto hostname = zero::os::hostname();
    REQUIRE(hostname);

    auto output = exec("hostname");
    REQUIRE(output);
    REQUIRE(zero::strings::trim(*output) == *hostname);
}

TEST_CASE("get username", "[os]") {
    auto username = zero::os::username();
    REQUIRE(username);

    auto output = exec("whoami");
    REQUIRE(output);
    REQUIRE(zero::strings::trim(*output) == *username);
}