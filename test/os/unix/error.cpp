#include <zero/os/unix/error.h>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>

TEST_CASE("unix error", "[unix]") {
    std::array<char, 1024> buffer = {};
    const auto result = zero::os::unix::expected([&] {
        return read(-1, buffer.data(), buffer.size());
    });
    REQUIRE(!result);
    REQUIRE(result.error() == std::errc::bad_file_descriptor);
}
