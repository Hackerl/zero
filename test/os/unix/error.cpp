#include <catch_extensions.h>
#include <zero/os/unix/error.h>
#include <unistd.h>

TEST_CASE("unix syscall wrapper", "[os::unix::error]") {
    const auto result = zero::os::unix::expected([&] {
        std::array<char, 1024> buffer{};
        return read(-1, buffer.data(), buffer.size());
    });
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == std::errc::bad_file_descriptor);
}
