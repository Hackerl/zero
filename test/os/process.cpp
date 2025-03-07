#include <catch_extensions.h>
#include <zero/os/process.h>
#include <zero/env.h>
#include <zero/defer.h>
#include <zero/os/os.h>
#include <zero/strings/strings.h>
#include <zero/filesystem/fs.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <fmt/format.h>
#include <ranges>

#ifdef _WIN32
#include <future>
#include <zero/os/windows/error.h>
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

TEST_CASE("list process ids", "[os::process]") {
    const auto ids = zero::os::process::all();
    REQUIRE(ids);

#ifdef _WIN32
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(static_cast<zero::os::process::ID>(GetCurrentProcessId())));
#else
    REQUIRE_THAT(*ids, Catch::Matchers::Contains(static_cast<zero::os::process::ID>(getpid())));
#endif
}

TEST_CASE("process", "[os::process]") {
    const auto process = zero::os::process::self();
    REQUIRE(process);

    const auto path = zero::filesystem::applicationPath();
    REQUIRE(path);

    SECTION("name") {
        REQUIRE(process->name() == path->filename());
    }

    SECTION("exe") {
        REQUIRE(process->exe() == *path);
    }

    SECTION("cmdline") {
        const auto cmdline = process->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path->filename().string()));
    }

    SECTION("cwd") {
        REQUIRE(process->cwd() == zero::filesystem::currentPath());
    }

    SECTION("envs") {
        const auto envs = process->envs();
        REQUIRE(envs);
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = process->startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 10min);
    }

    SECTION("memory") {
        const auto memory = process->memory();
        REQUIRE(memory);
    }

    SECTION("cpu") {
        const auto cpu = process->cpu();
        REQUIRE(cpu);
    }

    SECTION("io") {
        const auto io = process->io();
        REQUIRE(io);
    }
}

TEST_CASE("child process", "[os::process]") {
    const auto type = GENERATE(
        zero::os::process::Command::StdioType::NUL,
        zero::os::process::Command::StdioType::INHERIT,
        zero::os::process::Command::StdioType::PIPED
    );

    // If we don't consume data, the child process will be blocked on writing.
    auto child = zero::os::process::Command{PROGRAM}
                 .stdInput(type)
                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                 .stdError(type)
                 .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                 .spawn();
    REQUIRE(child);

    SECTION("stdio") {
        REQUIRE_FALSE(child->stdOutput());

        if (type == zero::os::process::Command::StdioType::PIPED) {
            REQUIRE(child->stdInput());
            REQUIRE(child->stdError());
        }
        else {
            REQUIRE_FALSE(child->stdInput());
            REQUIRE_FALSE(child->stdError());
        }

        REQUIRE(child->wait());
    }

    SECTION("try wait") {
        SECTION("running") {
            REQUIRE(child->tryWait() == std::nullopt);
            REQUIRE(child->wait());
        }

        SECTION("exited") {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2s);

            const auto status = child->tryWait();
            REQUIRE(status);
            REQUIRE(*status);
            REQUIRE(status->value().success());
        }
    }

    SECTION("wait") {
        const auto status = child->wait();
        REQUIRE(status);
        REQUIRE(status->success());
    }

    SECTION("name") {
        const auto name = child->name();
        REQUIRE(name);
        REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));
    }

    SECTION("exe") {
        const auto exe = child->exe();
        REQUIRE(exe);
        REQUIRE_THAT(exe->filename().string(), Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));
    }

    SECTION("cmdline") {
        const auto cmdline = child->cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));
        REQUIRE_THAT(
            (std::ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
            Catch::Matchers::RangeEquals(ARGUMENTS)
        );
    }

    SECTION("cwd") {
        REQUIRE(child->cwd() == zero::filesystem::currentPath());
    }

    SECTION("envs") {
        const auto envs = child->envs();
        REQUIRE(envs);
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = child->startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);
    }

    SECTION("memory") {
        const auto memory = child->memory();
        REQUIRE(memory);
    }

    SECTION("cpu") {
        const auto cpu = child->cpu();
        REQUIRE(cpu);
    }

    SECTION("io") {
        const auto io = child->io();
        REQUIRE(io);
    }
}

TEST_CASE("exit status", "[os::process]") {
    SECTION("raw") {
        const auto status = GENERATE(take(1, random<zero::os::process::ExitStatus::Native>(0, 1024)));
        REQUIRE(zero::os::process::ExitStatus{status}.raw() == status);
    }

    SECTION("success") {
        SECTION("yes") {
            REQUIRE(zero::os::process::ExitStatus{0}.success());
        }

        SECTION("no") {
            REQUIRE_FALSE(zero::os::process::ExitStatus{1}.success());
        }
    }

    SECTION("code") {
        const auto code = GENERATE(take(5, random<zero::os::process::ExitStatus::Native>(0, 255)));

#ifdef _WIN32
        REQUIRE(zero::os::process::ExitStatus{code}.code() == code);
#else
        SECTION("has") {
            REQUIRE(zero::os::process::ExitStatus{(code & 0xff) << 8}.code() == code);
        }

        SECTION("no") {
            const auto sig = GENERATE(take(5, random(1, 32)));
            REQUIRE(zero::os::process::ExitStatus{sig & 0x7f}.code() == std::nullopt);
        }
#endif
    }

#ifndef _WIN32
    SECTION("signal") {
        const auto sig = GENERATE(take(5, random(1, 32)));

        SECTION("has") {
            REQUIRE(zero::os::process::ExitStatus{sig & 0x7f}.signal() == sig);
        }

        SECTION("no") {
            REQUIRE(zero::os::process::ExitStatus{0}.signal() == std::nullopt);
        }
    }

    SECTION("stopped signal") {
        const auto sig = GENERATE(take(5, random(1, 32)));

        SECTION("has") {
            REQUIRE(zero::os::process::ExitStatus{(sig << 8) | 0x7f}.stoppedSignal() == sig);
        }

        SECTION("no") {
            REQUIRE(zero::os::process::ExitStatus{0}.stoppedSignal() == std::nullopt);
        }
    }

    SECTION("core dumped") {
        const auto sig = GENERATE(take(5, random(1, 32)));

        SECTION("yes") {
            REQUIRE(zero::os::process::ExitStatus{(sig & 0x7f) | (1 << 7)}.coreDumped());
        }

        SECTION("no") {
            REQUIRE_FALSE(zero::os::process::ExitStatus{0}.coreDumped());
        }
    }

    SECTION("continued") {
        SECTION("yes") {
#ifdef __APPLE__
            REQUIRE(zero::os::process::ExitStatus{(SIGCONT << 8) | 0x7f}.continued());
#else
            REQUIRE(zero::os::process::ExitStatus{0xffff}.continued());
#endif
        }

        SECTION("no") {
            REQUIRE_FALSE(zero::os::process::ExitStatus{0}.continued());
        }
    }
#endif
}

TEST_CASE("spawn child process with arguments", "[os::process]") {
    auto command = zero::os::process::Command{PROGRAM}
        .stdOutput(zero::os::process::Command::StdioType::NUL);

    SECTION("add arg") {
        for (const auto &arg: ARGUMENTS)
            command.arg(arg);
    }

    SECTION("add args") {
        command.args({ARGUMENTS.begin(), ARGUMENTS.end()});
    }

    auto child = command.spawn();
    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));

    const auto name = child->name();
    REQUIRE(name);
    REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(PROGRAM, Catch::CaseSensitive::No));
}

#ifdef _WIN32
TEST_CASE("spawn child process with complex escape characters", "[os::process]") {
    constexpr std::array args{"\t", "\"", " ", "\\", "\t\", \\", "a", "b", "c"};

    auto child = zero::os::process::Command{"findstr"}
                    .args({args.begin(), args.end()})
                    .stdInput(zero::os::process::Command::StdioType::NUL)
                    .stdOutput(zero::os::process::Command::StdioType::NUL)
                    .stdError(zero::os::process::Command::StdioType::NUL)
                    .spawn();
    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));

    const auto cmdline = child->cmdline();
    REQUIRE(cmdline);
    REQUIRE_THAT(*cmdline, Catch::Matchers::SizeIs(args.size() + 1));
    REQUIRE_THAT(
        (std::ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
        Catch::Matchers::RangeEquals(args)
    );
}
#endif

TEST_CASE("spawn child process with working directory", "[os::process]") {
    const auto temp = zero::filesystem::temporaryDirectory().and_then(zero::filesystem::canonical);
    REQUIRE(temp);

    auto child = zero::os::process::Command{PROGRAM}
                 .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                 .currentDirectory(*temp)
                 .stdOutput(zero::os::process::Command::StdioType::NUL)
                 .spawn();
    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));
    REQUIRE(child->cwd() == *temp);
}

TEST_CASE("spawn child process with environment", "[os::process]") {
#ifdef __APPLE__
    const auto output = zero::os::process::Command{"env"}
                        .clearEnv()
                        .env("ZERO_PROCESS_TESTS", "1")
                        .output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE_THAT(
        (std::string{reinterpret_cast<const char *>(output->out.data()), output->out.size()}),
        Catch::Matchers::ContainsSubstring("ZERO_PROCESS_TESTS")
    );
#else
    auto command = zero::os::process::Command{PROGRAM}
                   .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                   .stdOutput(zero::os::process::Command::StdioType::NUL);

    SECTION("inherit") {
        REQUIRE(zero::env::set("ZERO_PROCESS_TESTS", "1"));
        DEFER(REQUIRE(zero::env::unset("ZERO_PROCESS_TESTS")));

        auto child = command.spawn();
        REQUIRE(child);
        DEFER(REQUIRE(child->wait()));

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
    }

    SECTION("without inherit") {
        SECTION("empty") {
            auto child = command.clearEnv().spawn();
            REQUIRE(child);
            DEFER(REQUIRE(child->wait()));

            const auto envs = child->envs();
            REQUIRE(envs);
            REQUIRE_THAT(*envs, Catch::Matchers::IsEmpty());
        }

        SECTION("not empty") {
            REQUIRE(zero::env::set("ZERO_PROCESS_TESTS", "1"));
            DEFER(REQUIRE(zero::env::unset("ZERO_PROCESS_TESTS")));

            auto child = command.clearEnv().spawn();
            REQUIRE(child);
            DEFER(REQUIRE(child->wait()));

            const auto envs = child->envs();
            REQUIRE(envs);
            REQUIRE_THAT(std::views::keys(*envs), !Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        }

        SECTION("add env") {
            auto child = command.env("ZERO_PROCESS_TESTS", "1").spawn();
            REQUIRE(child);
            DEFER(REQUIRE(child->wait()));

            const auto envs = child->envs();
            REQUIRE(envs);
            REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
            REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
        }

        SECTION("remove env") {
            REQUIRE(zero::env::set("ZERO_PROCESS_TESTS", "1"));
            DEFER(REQUIRE(zero::env::unset("ZERO_PROCESS_TESTS")));

            auto child = command.removeEnv("ZERO_PROCESS_TESTS").spawn();
            REQUIRE(child);
            DEFER(REQUIRE(child->wait()));

            const auto envs = child->envs();
            REQUIRE(envs);
            REQUIRE_THAT(std::views::keys(*envs), !Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        }

        SECTION("set envs") {
            auto child = command.envs({{"ZERO_PROCESS_TESTS", "1"}}).spawn();
            REQUIRE(child);
            DEFER(REQUIRE(child->wait()));

            const auto envs = child->envs();
            REQUIRE(envs);
            REQUIRE_THAT(std::views::keys(*envs), Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
            REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
        }
    }
#endif
}

TEST_CASE("spawn child process with resource", "[os::process]") {
    using namespace std::chrono_literals;

    auto pipe = zero::os::pipe();
    REQUIRE(pipe);

    auto &[reader, writer] = *pipe;

    SECTION("inherit") {
        auto duplicate = writer.duplicate(true);
        REQUIRE(duplicate);

        auto child = zero::os::process::Command{PROGRAM}
                     .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                     .inheritedResource(*std::move(duplicate))
                     .stdOutput(zero::os::process::Command::StdioType::NUL)
                     .spawn();
        REQUIRE(child);
        DEFER(REQUIRE(child->wait()));

        const auto tp = std::chrono::system_clock::now();
        REQUIRE(writer.close());

#ifdef _WIN32
        const auto result = zero::os::windows::expected([&] {
            DWORD n{};
            std::array<char, 64> buffer{};
            return ReadFile(*reader, buffer.data(), buffer.size(), &n, nullptr);
        });
        REQUIRE_ERROR(result, std::errc::broken_pipe);
#else
            const auto n = zero::os::unix::expected([&] {
                std::array<char, 64> buffer{};
                return read(*reader, buffer.data(), buffer.size());
            });
            REQUIRE(n == 0);
#endif
        REQUIRE(std::chrono::system_clock::now() - tp > 0.9s);
    }

    SECTION("without inherit") {
        auto child = zero::os::process::Command{PROGRAM}
                     .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                     .stdOutput(zero::os::process::Command::StdioType::NUL)
                     .spawn();
        REQUIRE(child);
        DEFER(REQUIRE(child->wait()));

        const auto tp = std::chrono::system_clock::now();
        REQUIRE(writer.close());

#ifdef _WIN32
        const auto result = zero::os::windows::expected([&] {
            DWORD n{};
            std::array<char, 64> buffer{};
            return ReadFile(*reader, buffer.data(), buffer.size(), &n, nullptr);
        });
        REQUIRE_ERROR(result, std::errc::broken_pipe);
#else
        const auto n = zero::os::unix::expected([&] {
            std::array<char, 64> buffer{};
            return read(*reader, buffer.data(), buffer.size());
        });
        REQUIRE(n == 0);
#endif
        REQUIRE(std::chrono::system_clock::now() - tp < 0.9s);
    }
}

TEST_CASE("spawn child process with piped stdio", "[os::process]") {
#ifdef _WIN32
    auto child = zero::os::process::Command{"findstr"}
                 .arg("hello")
                 .stdInput(zero::os::process::Command::StdioType::PIPED)
                 .stdOutput(zero::os::process::Command::StdioType::PIPED)
                 .spawn();

    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));
    REQUIRE_FALSE(child->stdError());

    auto input = std::exchange(child->stdInput(), std::nullopt);
    REQUIRE(input);

    const auto output = std::exchange(child->stdOutput(), std::nullopt);
    REQUIRE(output);

    constexpr std::string_view data{"hello world"};

    DWORD n{};
    REQUIRE(WriteFile(input->get(), data.data(), data.size(), &n, nullptr));
    REQUIRE(n == data.size());
    REQUIRE(input->close());

    std::array<char, 64> buffer{};
    REQUIRE(ReadFile(output->get(), buffer.data(), buffer.size(), &n, nullptr));
    REQUIRE(n >= data.size());
    REQUIRE(data == zero::strings::trim(buffer.data()));
#else
    auto child = zero::os::process::Command{"cat"}
                    .stdInput(zero::os::process::Command::StdioType::PIPED)
                    .stdOutput(zero::os::process::Command::StdioType::PIPED)
                    .spawn();
    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));
    REQUIRE_FALSE(child->stdError());

    auto input = std::exchange(child->stdInput(), std::nullopt);
    REQUIRE(input);

    const auto output = std::exchange(child->stdOutput(), std::nullopt);
    REQUIRE(output);

    constexpr std::string_view data{"hello world"};

    auto n = zero::os::unix::ensure([&] {
        return write(input->get(), data.data(), data.size());
    });
    REQUIRE(n == data.size());
    REQUIRE(input->close());

    std::array<char, 64> buffer{};
    n = zero::os::unix::ensure([&] {
        return read(output->get(), buffer.data(), buffer.size());
    });
    REQUIRE(n == data.size());
    REQUIRE(data == buffer.data());
#endif
}

TEST_CASE("spawn child process and collect status", "[os::process]") {
    const auto status = zero::os::process::Command{PROGRAM}
                        .args({ARGUMENTS.begin(), ARGUMENTS.end()})
                        .stdOutput(zero::os::process::Command::StdioType::NUL)
                        .status();
    REQUIRE(status);
    REQUIRE(status->success());
}

TEST_CASE("spawn child process and collect output", "[os::process]") {
    const auto hostname = zero::os::hostname();
    REQUIRE(hostname);

    const auto output = zero::os::process::Command{"hostname"}.output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE(zero::strings::trim({
        reinterpret_cast<const char *>(output->out.data()),
        output->out.size()
    }) == *hostname);
}

TEST_CASE("spawn child process with pseudo console", "[os::process]") {
    auto pc = zero::os::process::PseudoConsole::make(80, 32);
    REQUIRE(pc);

    const std::string keyword{"hello"};
    constexpr std::string_view input{"echo hello\rexit\r"};

#ifdef _WIN32
    auto child = pc->spawn(zero::os::process::Command{"cmd"});
    REQUIRE(child);

    const auto &master = pc->master();
    REQUIRE(master);

    DWORD num{};
    REQUIRE(WriteFile(*master, input.data(), input.size(), &num, nullptr));
    REQUIRE(num == input.size());

    auto future = std::async([&]() -> std::expected<std::vector<char>, std::error_code> {
        std::vector<char> data;

        while (true) {
            DWORD n{};
            std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

            if (const auto result = zero::os::windows::expected([&] {
                return ReadFile(*master, buffer.data(), buffer.size(), &n, nullptr);
            }); !result) {
                if (result.error() != std::errc::broken_pipe)
                    return std::unexpected{result.error()};

                break;
            }

            assert(n > 0);
            data.insert(data.end(), buffer.begin(), buffer.begin() + n);
        }

        return data;
    });

    REQUIRE(child->wait());

    pc->close();

    const auto data = future.get();
    REQUIRE(data);
    REQUIRE_THAT((std::string{data->data(), data->size()}), Catch::Matchers::ContainsSubstring(keyword));
#else
    auto child = pc->spawn(zero::os::process::Command{"sh"});
    REQUIRE(child);
    DEFER(REQUIRE(child->wait()));

    const auto &master = pc->master();
    REQUIRE(master);

    auto n = zero::os::unix::ensure([&] {
        return write(*master, input.data(), input.size());
    });
    REQUIRE(n == input.size());

    std::vector<char> data;

    while (true) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        n = zero::os::unix::ensure([&] {
            return read(*master, buffer.data(), buffer.size());
        });

        if (!n) {
            if (n.error() != std::errc::io_error) {
                FAIL();
            }

            break;
        }

        if (*n == 0)
            break;

        data.insert(data.end(), buffer.begin(), buffer.begin() + *n);
    }

    REQUIRE_THAT((std::string{data.data(), data.size()}), Catch::Matchers::ContainsSubstring(keyword));
#endif
}
