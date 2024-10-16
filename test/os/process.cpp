#include <zero/os/process.h>
#include <zero/env.h>
#include <zero/os/os.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/fs.h>
#include <zero/filesystem/std.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <range/v3/view.hpp>
#include <fmt/format.h>

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

        const auto path = zero::filesystem::applicationPath();
        REQUIRE(path);

        const auto name = process->name();
        REQUIRE(name);
        REQUIRE(*name == path->filename());

        const auto exe = process->exe();
        REQUIRE(exe);
        REQUIRE(*exe == *path);

        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));

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
            REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE_THAT(
                exe->filename().string(),
                Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No)
            );

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE_THAT(*cmdline, Catch::Matchers::SizeIs(ARGUMENTS.size() + 1));
            REQUIRE_THAT(
                (ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
                Catch::Matchers::RangeEquals(ARGUMENTS)
            );

            REQUIRE(child->wait());
        }

        SECTION("add arg") {
            auto cmd = zero::os::process::Command(PROGRAM).stdOutput(zero::os::process::Command::StdioType::NUL);

            for (const auto &arg: ARGUMENTS)
                cmd = cmd.arg(arg);

            auto child = cmd.spawn();
            REQUIRE(child);

            const auto name = child->name();
            REQUIRE(name);
            REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE_THAT(
                exe->filename().string(),
                Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No)
            );

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE_THAT(*cmdline, Catch::Matchers::SizeIs(ARGUMENTS.size() + 1));
            REQUIRE_THAT(
                (ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
                Catch::Matchers::RangeEquals(ARGUMENTS)
            );

            REQUIRE(child->wait());
        }

        SECTION("set cwd") {
            const auto temp = zero::filesystem::temporaryDirectory().and_then(zero::filesystem::canonical);
            REQUIRE(temp);

            auto child = command
                         .currentDirectory(*temp)
                         .stdOutput(zero::os::process::Command::StdioType::NUL)
                         .spawn();
            REQUIRE(child);

            const auto cwd = child->cwd();
            REQUIRE(cwd);
            REQUIRE(*cwd == *temp);

            REQUIRE(child->wait());
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

            REQUIRE_THAT(
                (std::string{reinterpret_cast<const char *>(output->out.data()), output->out.size()}),
                Catch::Matchers::ContainsSubstring("ZERO_PROCESS_TESTS")
            );
#else
            SECTION("inherit") {
                REQUIRE(zero::env::set("ZERO_PROCESS_TESTS", "1"));

                auto child = command
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->find("ZERO_PROCESS_TESTS") != envs->end());
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                REQUIRE(child->wait());
                REQUIRE(zero::env::unset("ZERO_PROCESS_TESTS"));
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

                    REQUIRE(child->wait());
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
                    REQUIRE(envs->find("ZERO_PROCESS_TESTS") != envs->end());
                    REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                    REQUIRE(child->wait());
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
                REQUIRE(envs->find("ZERO_PROCESS_TESTS") != envs->end());
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                REQUIRE(child->wait());
            }

            SECTION("remove env") {
                REQUIRE(zero::env::set("ZERO_PROCESS_TESTS", "1"));

                auto child = command
                             .removeEnv("ZERO_PROCESS_TESTS")
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE_FALSE(envs->find("ZERO_PROCESS_TESTS") != envs->end());

                REQUIRE(child->wait());
                REQUIRE(zero::env::unset("ZERO_PROCESS_TESTS"));
            }

            SECTION("set envs") {
                auto child = command
                             .envs({{"ZERO_PROCESS_TESTS", "1"}})
                             .stdOutput(zero::os::process::Command::StdioType::NUL)
                             .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->find("ZERO_PROCESS_TESTS") != envs->end());
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                REQUIRE(child->wait());
            }
#endif
        }

#ifdef _WIN32
        SECTION("quote") {
            constexpr std::array args{"\t", "\"", " ", "\\", "\t\", \\", "a", "b", "c"};

            auto child = zero::os::process::Command("findstr")
                         .args({args.begin(), args.end()})
                         .stdInput(zero::os::process::Command::StdioType::NUL)
                         .stdOutput(zero::os::process::Command::StdioType::NUL)
                         .stdError(zero::os::process::Command::StdioType::NUL)
                         .spawn();
            REQUIRE(child);

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE_THAT(*cmdline, Catch::Matchers::SizeIs(args.size() + 1));
            REQUIRE_THAT(
                (ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
                Catch::Matchers::RangeEquals(args)
            );

            REQUIRE(child->wait());
        }
#endif

        SECTION("redirect") {
#ifdef _WIN32
            auto child = zero::os::process::Command("findstr")
                         .arg("hello")
                         .stdInput(zero::os::process::Command::StdioType::PIPED)
                         .stdOutput(zero::os::process::Command::StdioType::PIPED)
                         .spawn();

            REQUIRE(child);
            REQUIRE_FALSE(child->stdError());

            const auto input = std::exchange(child->stdInput(), std::nullopt);
            REQUIRE(input);

            const auto output = std::exchange(child->stdOutput(), std::nullopt);
            REQUIRE(output);

            constexpr std::string_view data{"hello world"};

            DWORD n{};
            REQUIRE(WriteFile(*input, data.data(), data.size(), &n, nullptr));
            REQUIRE(n == data.size());
            CloseHandle(*input);

            std::array<char, 64> buffer{};
            REQUIRE(ReadFile(*output, buffer.data(), buffer.size(), &n, nullptr));
            REQUIRE(n >= data.size());
            REQUIRE(data == zero::strings::trim(buffer.data()));
            CloseHandle(*output);

            REQUIRE(child->wait());
#else
            auto child = zero::os::process::Command("cat")
                         .stdInput(zero::os::process::Command::StdioType::PIPED)
                         .stdOutput(zero::os::process::Command::StdioType::PIPED)
                         .spawn();
            REQUIRE(child);
            REQUIRE_FALSE(child->stdError());

            const auto input = std::exchange(child->stdInput(), std::nullopt);
            REQUIRE(input);

            const auto output = std::exchange(child->stdOutput(), std::nullopt);
            REQUIRE(output);

            constexpr std::string_view data{"hello world"};

            auto n = zero::os::unix::ensure([&] {
                return write(*input, data.data(), data.size());
            });
            REQUIRE(n);
            REQUIRE(*n == data.size());
            close(*input);

            std::array<char, 64> buffer{};
            n = zero::os::unix::ensure([&] {
                return read(*output, buffer.data(), buffer.size());
            });
            REQUIRE(n);
            REQUIRE(*n == data.size());
            REQUIRE(data == buffer.data());
            close(*output);

            REQUIRE(child->wait());
#endif
        }

        SECTION("pseudo console") {
            auto pc = zero::os::process::PseudoConsole::make(80, 32);
            REQUIRE(pc);

            const std::string keyword{"hello"};
            constexpr std::string_view input{"echo hello\rexit\r"};

#ifdef _WIN32
            auto child = pc->spawn(zero::os::process::Command("cmd"));
            REQUIRE(child);

            const auto handle = pc->file();
            REQUIRE(handle);

            DWORD num{};
            REQUIRE(WriteFile(handle, input.data(), input.size(), &num, nullptr));
            REQUIRE(num == input.size());

            auto future = std::async([=]() -> tl::expected<std::vector<char>, std::error_code> {
                std::vector<char> data;

                while (true) {
                    DWORD n{};
                    std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

                    if (const auto result = zero::os::nt::expected([&] {
                        return ReadFile(handle, buffer.data(), buffer.size(), &n, nullptr);
                    }); !result) {
                        if (result.error() != std::errc::broken_pipe)
                            return tl::unexpected(result.error());

                        break;
                    }

                    assert(n > 0);
                    std::copy_n(buffer.begin(), n, std::back_inserter(data));
                }

                return data;
            });

            REQUIRE(child->wait());

            pc->close();

            const auto data = future.get();
            REQUIRE(data);
            REQUIRE_THAT((std::string{(data->data()), data->size()}), Catch::Matchers::ContainsSubstring(keyword));
#else
            auto child = pc->spawn(zero::os::process::Command("sh"));
            REQUIRE(child);

            const auto fd = pc->file();
            REQUIRE(fd >= 0);

            auto n = zero::os::unix::ensure([&] {
                return write(fd, input.data(), input.size());
            });
            REQUIRE(n);
            REQUIRE(*n == input.size());

            std::vector<char> data;

            while (true) {
                std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

                n = zero::os::unix::ensure([&] {
                    return read(fd, buffer.data(), buffer.size());
                });

                if (!n) {
                    if (n.error() != std::errc::io_error) {
                        FAIL();
                    }

                    break;
                }

                if (*n == 0)
                    break;

                std::copy_n(buffer.begin(), *n, std::back_inserter(data));
            }

            REQUIRE_THAT((std::string{data.data(), data.size()}), Catch::Matchers::ContainsSubstring(keyword));

            REQUIRE(child->wait());
#endif
        }

        SECTION("status") {
            const auto status = zero::os::process::Command("hostname").status();
            REQUIRE(status);
            REQUIRE(status->success());
        }

        SECTION("output") {
            SECTION("hostname") {
                const auto hostname = zero::os::hostname();
                REQUIRE(hostname);

                const auto output = zero::os::process::Command("hostname").output();
                REQUIRE(output);
                REQUIRE(output->status.success());
                REQUIRE(fmt::to_string(output->status) == "exit code(0)");

                REQUIRE(zero::strings::trim({
                    reinterpret_cast<const char *>(output->out.data()),
                    output->out.size()
                }) == *hostname);
            }

            SECTION("whoami") {
                const auto username = zero::os::username();
                REQUIRE(username);

                const auto output = zero::os::process::Command("whoami").output();
                REQUIRE(output);
                REQUIRE(output->status.success());
                REQUIRE(fmt::to_string(output->status) == "exit code(0)");

                REQUIRE_THAT(
                    (std::string{reinterpret_cast<const char *>(output->out.data()),output->out.size()}),
                    Catch::Matchers::ContainsSubstring(*username)
                );
            }
        }
    }
}
