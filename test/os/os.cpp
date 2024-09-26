#include <zero/os/os.h>
#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

TEST_CASE("get hostname", "[os]") {
    const auto hostname = zero::os::hostname();
    REQUIRE(hostname);

    const auto output = zero::os::process::Command("hostname").output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE(zero::strings::trim({reinterpret_cast<const char *>(output->out.data()), output->out.size()}) == *hostname);
}

TEST_CASE("get username", "[os]") {
    const auto username = zero::os::username();
    REQUIRE(username);

    const auto output = zero::os::process::Command("whoami").output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE_THAT(
        (std::string{reinterpret_cast<const char *>(output->out.data()), output->out.size()}),
        Catch::Matchers::ContainsSubstring(*username)
    );
}
