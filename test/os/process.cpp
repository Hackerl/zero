#include <zero/os/process.h>
#include <zero/os/os.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
constexpr auto SLEEP_PROGRAM = "timeout";
#else
constexpr auto SLEEP_PROGRAM = "sleep";
#endif

constexpr auto SLEEP_ARGUMENT = "1";

TEST_CASE("process module", "[process]") {
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
        auto command = zero::os::process::Command(SLEEP_PROGRAM).arg(SLEEP_ARGUMENT);
        REQUIRE(command.program() == SLEEP_PROGRAM);

        SECTION("spawn") {
            const auto child = command
                               .stdOutput(zero::os::process::Command::StdioType::NUL)
                               .spawn();
            REQUIRE(child);

            const auto name = child->name();
            REQUIRE(name);
            REQUIRE(name->starts_with(SLEEP_PROGRAM));

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE(exe->filename().string().starts_with(SLEEP_PROGRAM));

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE(cmdline->size() == 2);
            REQUIRE(cmdline->at(0) == SLEEP_PROGRAM);
            REQUIRE(cmdline->at(1) == SLEEP_ARGUMENT);

            const auto result = child->wait();
            REQUIRE(result);
        }

        SECTION("set args") {
            const auto child = command
                               .args({"2"})
                               .stdOutput(zero::os::process::Command::StdioType::NUL)
                               .spawn();
            REQUIRE(child);

            const auto name = child->name();
            REQUIRE(name);
            REQUIRE(name->starts_with(SLEEP_PROGRAM));

            const auto exe = child->exe();
            REQUIRE(exe);
            REQUIRE(exe->filename().string().starts_with(SLEEP_PROGRAM));

            const auto cmdline = child->cmdline();
            REQUIRE(cmdline);
            REQUIRE(cmdline->size() == 2);
            REQUIRE(cmdline->at(0) == SLEEP_PROGRAM);
            REQUIRE(cmdline->at(1) == "2");

            const auto result = child->wait();
            REQUIRE(result);
        }

        SECTION("set cwd") {
            const auto temp = std::filesystem::temp_directory_path();
            const auto child = command
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

            REQUIRE(WIFEXITED(output->status));
            REQUIRE(WEXITSTATUS(output->status) == 0);

            const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
            REQUIRE(result.find("ZERO_PROCESS_TESTS") != std::string::npos);
#else
            SECTION("inherit") {
                std::string env = "ZERO_PROCESS_TESTS=1";
#ifdef _WIN32
                _putenv(env.data());
#else
                putenv(env.data());
#endif

                const auto child = command
                                   .stdOutput(zero::os::process::Command::StdioType::NUL)
                                   .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(envs->contains("ZERO_PROCESS_TESTS"));
                REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");

                const auto result = child->wait();
                REQUIRE(result);

                env = "ZERO_PROCESS_TESTS=";
#ifdef _WIN32
                _putenv(env.data());
#else
                putenv(env.data());
#endif
            }

            SECTION("without inherit") {
                SECTION("empty") {
                    const auto child = command
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
                    const auto child = command
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
                const auto child = command
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
                std::string env = "ZERO_PROCESS_TESTS=1";
#ifdef _WIN32
                _putenv(env.data());
#else
                putenv(env.data());
#endif

                const auto child = command
                                   .removeEnv("ZERO_PROCESS_TESTS")
                                   .stdOutput(zero::os::process::Command::StdioType::NUL)
                                   .spawn();
                REQUIRE(child);

                const auto envs = child->envs();
                REQUIRE(envs);
                REQUIRE(!envs->contains("ZERO_PROCESS_TESTS"));

                const auto result = child->wait();
                REQUIRE(result);

                env = "ZERO_PROCESS_TESTS=";
#ifdef _WIN32
                _putenv(env.data());
#else
                putenv(env.data());
#endif
            }

            SECTION("set envs") {
                const auto child = command
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

        SECTION("output") {
            SECTION("hostname") {
                const auto hostname = zero::os::hostname();
                REQUIRE(hostname);

                const auto output = zero::os::process::Command("hostname").output();
                REQUIRE(output);
#ifdef _WIN32
                REQUIRE(output->status == 0);
#else
                REQUIRE(WIFEXITED(output->status));
                REQUIRE(WEXITSTATUS(output->status) == 0);
#endif
                const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
                REQUIRE(zero::strings::trim(result) == *hostname);
            }

            SECTION("whoami") {
                const auto username = zero::os::username();
                REQUIRE(username);

                const auto output = zero::os::process::Command("whoami").output();
                REQUIRE(output);
#ifdef _WIN32
                REQUIRE(output->status == 0);
#else
                REQUIRE(WIFEXITED(output->status));
                REQUIRE(WEXITSTATUS(output->status) == 0);
#endif
                const std::string result = {reinterpret_cast<const char *>(output->out.data()), output->out.size()};
                REQUIRE(zero::strings::trim(result).find(*username) != std::string::npos);
            }
        }
    }
}
