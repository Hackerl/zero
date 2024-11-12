#include <zero/os/unix/error.h>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>

TEST_CASE("unix syscall wrapper", "[unix]") {
    const auto result = zero::os::unix::expected([&] {
        std::array<char, 1024> buffer{};
        return read(-1, buffer.data(), buffer.size());
    });
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::bad_file_descriptor);
}
