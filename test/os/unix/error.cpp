#include <zero/os/unix/error.h>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>

TEST_CASE("unix error", "[unix]") {
    char buffer[1024];
    const auto result = zero::os::unix::expect([&] {
        return read(-1, buffer, sizeof(buffer));
    });
    REQUIRE(!result);
    REQUIRE(result.error() == std::errc::bad_file_descriptor);
}
