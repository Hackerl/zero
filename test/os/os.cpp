#include <zero/os/os.h>
#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("get hostname", "[os]") {
    const auto hostname = zero::os::hostname();
    REQUIRE(hostname);

    const auto output = zero::os::process::Command("hostname").output();
    REQUIRE(output);

#ifdef _WIN32
    REQUIRE(output->status == 0);
#else
    REQUIRE(WIFEXITED(output->status));
    REQUIRE(WEXITSTATUS(output->status) == 0);
#endif

    const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
    REQUIRE(zero::strings::trim(result) == *hostname);
}

TEST_CASE("get username", "[os]") {
    const auto username = zero::os::username();
    REQUIRE(username);

    const auto output = zero::os::process::Command("whoami").output();
    REQUIRE(output);

#ifdef _WIN32
    REQUIRE(output->status == 0);
#else
    REQUIRE(WIFEXITED(output->status));
    REQUIRE(WEXITSTATUS(output->status) == 0);
#endif

    const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
    REQUIRE(zero::strings::trim(result).find(*username) != std::string::npos);
}
