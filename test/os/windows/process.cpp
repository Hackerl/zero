#include <catch_extensions.h>
#include <zero/os/process.h>
#include <zero/os/os.h>
#include <zero/filesystem/fs.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>

TEST_CASE("list process ids - Windows", "[os::windows::process]") {
    const auto ids = zero::os::windows::process::all();
    REQUIRE(ids);
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(GetCurrentProcessId()));
}

TEST_CASE("process - Windows", "[os::windows::process]") {
    constexpr std::string program{"ping"};
    constexpr std::array arguments{"localhost", "-n", "2"};

    auto child = zero::os::process::Command{program}
                       .args({arguments.begin(), arguments.end()})
                       .env("ZERO_WINDOWS_PROCESS_TESTS", "1")
                       .stdInput(zero::os::process::Command::StdioType::NUL)
                       .stdOutput(zero::os::process::Command::StdioType::NUL)
                       .stdError(zero::os::process::Command::StdioType::NUL)
                       .spawn();
    REQUIRE(child);

    auto process = zero::os::windows::process::open(child->pid());
    REQUIRE(process);

    SECTION("handle") {
        const auto &handle = process->handle();
        REQUIRE(handle);
        REQUIRE(handle.valid());
    }

    SECTION("pid") {
        REQUIRE(process->pid() == GetProcessId(*process->handle()));
    }

    SECTION("ppid") {
        REQUIRE(process->ppid() == GetCurrentProcessId());
    }

    SECTION("name") {
        const auto name = process->name();
        REQUIRE(name);
        REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(program, Catch::CaseSensitive::No));
    }

    SECTION("cwd") {
        REQUIRE(process->cwd() == zero::filesystem::currentPath());
    }

    SECTION("exe") {
        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE_THAT(exe->filename().string(), Catch::Matchers::ContainsSubstring(program, Catch::CaseSensitive::No));
    }

    SECTION("cmdline") {
        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(program, Catch::CaseSensitive::No));
        REQUIRE_THAT(
            (std::ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
            Catch::Matchers::RangeEquals(arguments)
        );
    }

    SECTION("envs") {
        const auto envs = process->envs();
        REQUIRE(envs);
        REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_WINDOWS_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_WINDOWS_PROCESS_TESTS") == "1");
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = process->startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);
    }

    SECTION("cpu") {
        const auto cpu = process->cpu();
        REQUIRE(cpu);
    }

    SECTION("memory") {
        const auto memory = process->memory();
        REQUIRE(memory);
    }

    SECTION("io") {
        const auto io = process->io();
        REQUIRE(io);
    }

    SECTION("user") {
        const auto user = process->user();
        REQUIRE(user);

        const auto username = zero::os::username();
        REQUIRE(username);

        REQUIRE_THAT(*user, Catch::Matchers::EndsWith(*username));
    }

    SECTION("exit code") {
        SECTION("running") {
            REQUIRE_ERROR(process->exitCode(), zero::os::windows::process::Process::Error::PROCESS_STILL_ACTIVE);
        }

        SECTION("exited") {
            REQUIRE(process->wait(std::nullopt));
            REQUIRE(process->exitCode() == 0);
        }
    }

    SECTION("wait") {
        SECTION("timeout") {
            using namespace std::chrono_literals;
            REQUIRE_ERROR(process->wait(10ms), std::errc::timed_out);
        }

        SECTION("success") {
            REQUIRE(process->wait(std::nullopt));
        }
    }

    SECTION("terminate") {
        REQUIRE(process->terminate(EXIT_FAILURE));
        REQUIRE(process->wait(std::nullopt));
        REQUIRE(process->exitCode() == EXIT_FAILURE);
    }

    REQUIRE(child->wait());
}
