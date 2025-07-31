#include <catch_extensions.h>
#include <zero/os/unix/error.h>
#include <unistd.h>

TEST_CASE("unix syscall wrapper", "[os::unix::error]") {
    SECTION("failure") {
        const auto result = zero::os::unix::expected([] {
            errno = EINVAL;
            return -1;
        });
        REQUIRE_ERROR(result, std::errc::invalid_argument);
    }

    SECTION("success") {
        const auto result = zero::os::unix::expected([] {
            return 0;
        });
        REQUIRE(result);
    }
}

TEST_CASE("signal-safe unix syscall wrapper", "[os::unix::error]") {
    bool interrupted{false};

    const auto result = zero::os::unix::ensure([&] {
        if (!interrupted) {
            interrupted = true;
            errno = EINTR;
            return -1;
        }

        return 0;
    });
    REQUIRE(result);
    REQUIRE(interrupted);
}
