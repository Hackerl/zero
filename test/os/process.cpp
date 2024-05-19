#include <zero/os/process.h>
#include <zero/os/os.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <future>
#include <zero/os/nt/error.h>
#else
#include <unistd.h>
#include <zero/os/unix/error.h>
#endif

#ifdef _WIN32
constexpr auto PROGRAM = "ping";
constexpr auto ARGUMENTS = {"localhost", "-n", "2"};
#else
constexpr auto PROGRAM = "sleep";
constexpr auto ARGUMENTS = {"1"};
#endif

TEST_CASE("process", "[os]") {
    SECTION("process") {
        const auto ids = zero::os::process::all();
        REQUIRE(ids);

        const auto process = zero::os::process::self();
        REQUIRE(process);

        const auto path = zero::filesystem::getApplicationPath();
        REQUIRE(path);

        const auto name = process->name();
        REQUIRE(name);
        REQUIRE(*name == path->filename());

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE(cmdline->at(0).find(path->filename().string()) != std::string::npos);

        const auto cwd = process->cwd();
        REQUIRE(cwd);
        REQUIRE(*cwd == std::filesystem::current_path());

        const auto envs = process->envs();
        REQUIRE(envs);

        const auto memory = process->memory();
        REQUIRE(memory);

        const auto cpu = process->cpu();
        REQUIRE(cpu);

        const auto io = process->io();
        REQUIRE(io);
    }

    SECTION("command") {
        auto command = zero::os::process::Command(PROGRAM).args({ARGUMENTS.begin(), ARGUMENTS.end()});
        REQUIRE(command.program() == PROGRAM);

        SECTION("spawn") {
            auto child = command
                         .stdOutput(zero::os::process::Command::StdioType::NUL)
                         .spawn();
            REQUIRE(child);

            const auto name = child->name();
            REQUIRE(name);
            REQUIRE(zero::strings::tolower(*name).find(PROGRAM) != std::string::npos);

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE(zero::strings::tolower(exe->filename().string()).find(PROGRAM) != std::string::npos);

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE(cmdline->size() == ARGUMENTS.size() + 1);
            REQUIRE(std::equal(cmdline->begin() + 1, cmdline->end(), ARGUMENTS.begin()));

            const auto result = child->wait();
            REQUIRE(result);
        }

        SECTION("add arg") {
            auto cmd = zero::os::process::Command(PROGRAM).stdOutput(zero::os::process::Command::StdioType::NUL);

            for (const auto &arg: ARGUMENTS)
                cmd = cmd.arg(arg);

            auto child = cmd.spawn();
            REQUIRE(child);

            const auto name = child->name();
            REQUIRE(name);
            REQUIRE(zero::strings::tolower(*name).find(PROGRAM) != std::string::npos);

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE(zero::strings::tolower(exe->filename().string()).find(PROGRAM) != std::string::npos);

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE(cmdline->size() == ARGUMENTS.size() + 1);
            REQUIRE(std::equal(cmdline->begin() + 1, cmdline->end(), ARGUMENTS.begin()));

            const auto result = child->wait();
            REQUIRE(result);
        }

        SECTION("set cwd") {
            const auto temp = std::filesystem::temp_directory_path();
            auto child = command
                         .currentDirectory(temp)
                         .stdOutput(zero::os::process::Command::StdioType::NUL)
                         .spawn();
            REQUIRE(child);

            const auto cwd = child->cwd();
            REQUIRE(cwd);
            REQUIRE(*cwd == std::filesystem::canonical(temp));

            const auto result = child->wait();
            REQUIRE(result);
        }

        SECTION("env") {
#ifdef __APPLE__
            const auto output = zero::os::process::Command("env")
                                .clearEnv()
                                .env("ZERO_PROCESS_TESTS", "1")
                                .output();
            REQUIRE(output);
            REQUIRE(output->status.success());
            REQUIRE(fmt::to_string(output->status) == "exit code(0)");

            const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
            REQUIRE(result.find("ZERO_PROCESS_TESTS") != std::string::npos);
#else
            SECTION("inherit") {
#ifdef _WIN32
                SetEnvironmentVariableA("ZERO_PROCESS_TESTS", "1");
#else
                setenv("ZERO_PROCESS_TESTS", "1", 0);
#endif

                auto child = command
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->contains("ZERO_PROCESS_TESTS"));
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                const auto result = child->wait();
                REQUIRE(result);

#ifdef _WIN32
                SetEnvironmentVariableA("ZERO_PROCESS_TESTS", nullptr);
#else
                unsetenv("ZERO_PROCESS_TESTS");
#endif
            }

            SECTION("without inherit") {
                SECTION("empty") {
                    auto child = command
                                 .clearEnv()
                                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                                 .spawn();
                    REQUIRE(child);

                    const auto envs = child->envs();
                    REQUIRE(envs);
                    REQUIRE(envs->empty());

                    const auto result = child->wait();
                    REQUIRE(result);
                }

                SECTION("not empty") {
                    auto child = command
                                 .clearEnv()
                                 .env("ZERO_PROCESS_TESTS", "1")
                                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                                 .spawn();
                    REQUIRE(child);

                    const auto envs = child->envs();
                    REQUIRE(envs);
                    REQUIRE(envs->contains("ZERO_PROCESS_TESTS"));
                    REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                    const auto result = child->wait();
                    REQUIRE(result);
                }
            }

            SECTION("add env") {
                auto child = command
                             .env("ZERO_PROCESS_TESTS", "1")
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->contains("ZERO_PROCESS_TESTS"));
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                const auto result = child->wait();
                REQUIRE(result);
            }

            SECTION("remove env") {
#ifdef _WIN32
                SetEnvironmentVariableA("ZERO_PROCESS_TESTS", "1");
#else
                setenv("ZERO_PROCESS_TESTS", "1", 0);
#endif

                auto child = command
                             .removeEnv("ZERO_PROCESS_TESTS")
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(!envs->contains("ZERO_PROCESS_TESTS"));

                const auto result = child->wait();
                REQUIRE(result);

#ifdef _WIN32
                SetEnvironmentVariableA("ZERO_PROCESS_TESTS", nullptr);
#else
                unsetenv("ZERO_PROCESS_TESTS");
#endif
            }

            SECTION("set envs") {
                auto child = command
                             .envs({{"ZERO_PROCESS_TESTS", "1"}})
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->contains("ZERO_PROCESS_TESTS"));
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                const auto result = child->wait();
                REQUIRE(result);
            }
#endif
        }

#ifdef _WIN32
        SECTION("quote") {
            constexpr std::array args = {"\t", "\"", " ", "\\", "\t\", \\", "a", "b", "c"};

            auto child = zero::os::process::Command("findstr")
                         .args({args.begin(), args.end()})
                         .stdInput(zero::os::process::Command::StdioType::NUL)
                         .stdOutput(zero::os::process::Command::StdioType::NUL)
                         .stdError(zero::os::process::Command::StdioType::NUL)
                         .spawn();
            REQUIRE(child);

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE(cmdline->size() == args.size() + 1);
            REQUIRE(std::equal(cmdline->begin() + 1, cmdline->end(), args.begin()));

            const auto result = child->wait();
            REQUIRE(result);
        }
#endif

        SECTION("redirect") {
            using namespace std::string_view_literals;

#ifdef _WIN32
            auto child = zero::os::process::Command("findstr")
                         .arg("hello")
                         .stdInput(zero::os::process::Command::StdioType::PIPED)
                         .stdOutput(zero::os::process::Command::StdioType::PIPED)
                         .spawn();

            REQUIRE(child);
            REQUIRE(!child->stdError());

            const auto input = std::exchange(child->stdInput(), std::nullopt);
            REQUIRE(input);

            const auto output = std::exchange(child->stdOutput(), std::nullopt);
            REQUIRE(output);

            constexpr auto data = "hello wolrd"sv;

            DWORD n;
            REQUIRE(WriteFile(*input, data.data(), data.size(), &n, nullptr));
            REQUIRE(n == data.size());
            CloseHandle(*input);

            char buffer[64] = {};
            REQUIRE(ReadFile(*output, buffer, sizeof(buffer), &n, nullptr));
            REQUIRE(n >= data.size());
            REQUIRE(data == zero::strings::trim(buffer));
            CloseHandle(*output);

            const auto result = child->wait();
            REQUIRE(result);
#else
            auto child = zero::os::process::Command("cat")
                         .stdInput(zero::os::process::Command::StdioType::PIPED)
                         .stdOutput(zero::os::process::Command::StdioType::PIPED)
                         .spawn();
            REQUIRE(child);
            REQUIRE(!child->stdError());

            const auto input = std::exchange(child->stdInput(), std::nullopt);
            REQUIRE(input);

            const auto output = std::exchange(child->stdOutput(), std::nullopt);
            REQUIRE(output);

            constexpr auto data = "hello wolrd"sv;
            auto n = zero::os::unix::ensure([&] {
                return write(*input, data.data(), data.size());
            });
            REQUIRE(n);
            REQUIRE(*n == data.size());
            close(*input);

            char buffer[64] = {};
            n = zero::os::unix::ensure([&] {
                return read(*output, buffer, sizeof(buffer));
            });
            REQUIRE(n);
            REQUIRE(*n == data.size());
            REQUIRE(data == buffer);
            close(*output);

            const auto result = child->wait();
            REQUIRE(result);
#endif
        }

        SECTION("pseudo console") {
            using namespace std::string_view_literals;

            auto pc = zero::os::process::PseudoConsole::make(80, 32);
            REQUIRE(pc);

            constexpr auto data = "echo hello\rexit\r"sv;
            constexpr auto keyword = "hello"sv;

#ifdef _WIN32
            auto child = zero::os::process::Command("cmd").spawn(*pc);
            REQUIRE(child);

            const auto input = pc->input();
            REQUIRE(input);

            const auto output = pc->output();
            REQUIRE(output);

            DWORD num;
            REQUIRE(WriteFile(input, data.data(), data.size(), &num, nullptr));
            REQUIRE(num == data.size());

            auto future = std::async([=] {
                std::expected<std::vector<char>, std::error_code> result;

                while (true) {
                    DWORD n;
                    char buffer[1024];

                    const auto res = zero::os::nt::expected([&] {
                        return ReadFile(output, buffer, sizeof(buffer), &n, nullptr);
                    });

                    if (!res) {
                        if (res.error() == std::errc::broken_pipe)
                            break;

                        result = std::unexpected(res.error());
                        break;
                    }

                    assert(n > 0);
                    std::copy_n(buffer, n, std::back_inserter(*result));
                }

                return result;
            });

            const auto result = child->wait();
            REQUIRE(result);
            pc->close();

            const auto content = future.get();
            REQUIRE(content);
            REQUIRE(std::ranges::search(*content, keyword));
#else
            auto child = zero::os::process::Command("sh").spawn(*pc);
            REQUIRE(child);

            const int fd = pc->fd();
            auto n = zero::os::unix::ensure([&] {
                return write(fd, data.data(), data.size());
            });
            REQUIRE(n);
            REQUIRE(*n == data.size());

            std::vector<char> content;

            while (true) {
                char buffer[1024];
                n = zero::os::unix::ensure([&] {
                    return read(fd, buffer, sizeof(buffer));
                });

                if (!n) {
                    if (n.error() == std::errc::io_error)
                        break;

                    FAIL();
                }

                if (*n == 0)
                    break;

                std::copy_n(buffer, *n, std::back_inserter(content));
            }

            REQUIRE(std::ranges::search(content, keyword));

            const auto result = child->wait();
            REQUIRE(result);
#endif
        }

        SECTION("output") {
            SECTION("hostname") {
                const auto hostname = zero::os::hostname();
                REQUIRE(hostname);

                const auto output = zero::os::process::Command("hostname").output();
                REQUIRE(output);
                REQUIRE(output->status.success());
                REQUIRE(fmt::to_string(output->status) == "exit code(0)");

                const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
                REQUIRE(zero::strings::trim(result) == *hostname);
            }

            SECTION("whoami") {
                const auto username = zero::os::username();
                REQUIRE(username);

                const auto output = zero::os::process::Command("whoami").output();
                REQUIRE(output);
                REQUIRE(output->status.success());
                REQUIRE(fmt::to_string(output->status) == "exit code(0)");

                const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
                REQUIRE(zero::strings::trim(result).find(*username) != std::string::npos);
            }
        }
    }
}
