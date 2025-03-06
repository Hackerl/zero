#include <catch_extensions.h>
#include <zero/os/os.h>
#include <zero/expect.h>
#include <zero/os/process.h>
#include <zero/strings/strings.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <future>

#ifdef _WIN32
#include <zero/os/windows/error.h>
#else
#include <unistd.h>
#include <zero/os/unix/error.h>
#endif

namespace {
    std::expected<std::vector<std::byte>, std::error_code> readAll(const zero::os::Resource &resource) {
        std::vector<std::byte> data;

#ifdef _WIN32
        while (true) {
            DWORD n{};
            std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

            if (const auto result = zero::os::windows::expected([&] {
                return ReadFile(*resource, buffer.data(), buffer.size(), &n, nullptr);
            }); !result) {
                if (result.error() != std::errc::broken_pipe)
                    return std::unexpected{result.error()};

                break;
            }

            assert(n > 0);
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        }
#else
        while (true) {
            std::array<std::byte, 1024> buffer; // NOLINT(*-pro-type-member-init)

            const auto n = zero::os::unix::ensure([&] {
                return read(*resource, buffer.data(), buffer.size());
            });
            EXPECT(n);

            if (*n == 0)
                break;

            data.insert(data.end(), buffer.begin(), buffer.begin() + *n);
        }
#endif

        return data;
    }
}

TEST_CASE("get hostname", "[os]") {
    const auto hostname = zero::os::hostname();
    REQUIRE(hostname);

    const auto output = zero::os::process::Command{"hostname"}.output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE(zero::strings::trim({reinterpret_cast<const char *>(output->out.data()), output->out.size()}) == *hostname);
}

TEST_CASE("get username", "[os]") {
    const auto username = zero::os::username();
    REQUIRE(username);

    const auto output = zero::os::process::Command{"whoami"}.output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE_THAT(
        (std::string{reinterpret_cast<const char *>(output->out.data()), output->out.size()}),
        Catch::Matchers::ContainsSubstring(*username)
    );
}

TEST_CASE("anonymous pipe", "[os]") {
    auto pipe = zero::os::pipe();
    REQUIRE(pipe);

    auto &[reader, writer] = *pipe;

    auto future = std::async(readAll, std::cref(reader));
    const auto input = GENERATE(take(100, randomBytes(1, 10240)));

#ifdef _WIN32
    DWORD n{};
    REQUIRE(zero::os::windows::expected([&] {
        return WriteFile(*writer, input.data(), static_cast<DWORD>(input.size()), &n, nullptr);
    }));
    REQUIRE(n == input.size());
#else
    REQUIRE(zero::os::unix::ensure([&] {
        return write(*writer, input.data(), input.size());
    }) == input.size());
#endif

    REQUIRE(writer.close());
    REQUIRE(future.get() == input);
}
