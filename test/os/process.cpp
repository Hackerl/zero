#include <catch_extensions.h>
#include <zero/os/process.h>
#include <zero/env.h>
#include <zero/defer.h>
#include <zero/os/os.h>
#include <zero/strings.h>
#include <zero/filesystem.h>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <ranges>
#include <future>

#ifndef _WIN32
#include <csignal>
#endif

#ifdef _WIN32
constexpr auto Program = "ping";
constexpr auto Arguments = {"localhost", "-n", "2"};
#else
constexpr auto Program = "sleep";
constexpr auto Arguments = {"1"};
#endif

TEST_CASE("list process ids", "[os::process]") {
    REQUIRE_THAT(zero::os::process::all(), Catch::Matchers::Contains(zero::os::process::currentProcessID()));
}

TEST_CASE("process", "[os::process]") {
    const auto process = zero::os::process::self();
    const auto path = zero::filesystem::applicationPath();

    SECTION("name") {
        REQUIRE(process.name() == path.filename());
    }

    SECTION("exe") {
        REQUIRE(process.exe() == path);
    }

    SECTION("cmdline") {
        const auto cmdline = process.cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(path.filename().string()));
    }

    SECTION("cwd") {
        REQUIRE(process.cwd() == zero::filesystem::currentPath());
    }

    SECTION("envs") {
        const auto envs = process.envs();
        REQUIRE(envs);
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = process.startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 10min);
    }

    SECTION("memory") {
        const auto memory = process.memory();
        REQUIRE(memory);
    }

    SECTION("cpu") {
        const auto cpu = process.cpu();
        REQUIRE(cpu);
    }

    SECTION("io") {
        const auto io = process.io();
        REQUIRE(io);
    }

    SECTION("user") {
        const auto user = process.user();
        REQUIRE(user);
        REQUIRE_THAT(*user, Catch::Matchers::EndsWith(zero::error::guard(zero::os::username())));
    }
}

TEST_CASE("child process", "[os::process]") {
    const auto type = GENERATE(
        zero::os::process::Command::StdioType::Null,
        zero::os::process::Command::StdioType::Inherit,
        zero::os::process::Command::StdioType::Piped
    );

    // If we don't consume data, the child process will be blocked on writing.
    auto child = zero::error::guard(
        zero::os::process::Command{Program}
        .stdInput(type)
        .stdOutput(zero::os::process::Command::StdioType::Null)
        .stdError(type)
        .args({Arguments.begin(), Arguments.end()})
        .spawn()
    );

    SECTION("stdio") {
        REQUIRE_FALSE(child.stdOutput());

        if (type == zero::os::process::Command::StdioType::Piped) {
            REQUIRE(child.stdInput());
            REQUIRE(child.stdError());
        }
        else {
            REQUIRE_FALSE(child.stdInput());
            REQUIRE_FALSE(child.stdError());
        }

        child.wait();
    }

    SECTION("try wait") {
        SECTION("running") {
            REQUIRE_FALSE(child.tryWait());
            child.wait();
        }

        SECTION("exited") {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2s);

            const auto status = child.tryWait();
            REQUIRE(status);
            REQUIRE(status->success());
        }
    }

    SECTION("wait") {
        const auto status = child.wait();
        REQUIRE(status.success());
    }

    SECTION("name") {
        const auto name = child.name();
        REQUIRE(name);
        REQUIRE_THAT(*name, Catch::Matchers::ContainsSubstring(Program, Catch::CaseSensitive::No));
    }

    SECTION("exe") {
        const auto exe = child.exe();
        REQUIRE(exe);
        REQUIRE_THAT(exe->filename().string(), Catch::Matchers::ContainsSubstring(Program, Catch::CaseSensitive::No));
    }

    SECTION("cmdline") {
        const auto cmdline = child.cmdline();
        REQUIRE(cmdline);
        REQUIRE_THAT(cmdline->at(0), Catch::Matchers::ContainsSubstring(Program, Catch::CaseSensitive::No));
        REQUIRE_THAT(
            (std::ranges::subrange{cmdline->begin() + 1, cmdline->end()}),
            Catch::Matchers::RangeEquals(Arguments)
        );
    }

    SECTION("cwd") {
        REQUIRE(child.cwd() == zero::filesystem::currentPath());
    }

    SECTION("envs") {
        const auto envs = child.envs();
        REQUIRE(envs);
    }

    SECTION("start time") {
        using namespace std::chrono_literals;
        const auto startTime = child.startTime();
        REQUIRE(startTime);
        REQUIRE(std::chrono::system_clock::now() - *startTime < 1min);
    }

    SECTION("memory") {
        const auto memory = child.memory();
        REQUIRE(memory);
    }

    SECTION("cpu") {
        const auto cpu = child.cpu();
        REQUIRE(cpu);
    }

    SECTION("io") {
        const auto io = child.io();
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
            const auto sig = GENERATE(
                SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGKILL,
                SIGABRT, SIGSEGV, SIGILL, SIGFPE, SIGBUS
            );
            REQUIRE_FALSE(zero::os::process::ExitStatus{sig & 0x7f}.code());
        }
#endif
    }

#ifndef _WIN32
    SECTION("signal") {
        const auto sig = GENERATE(SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGKILL, SIGABRT, SIGSEGV, SIGILL, SIGFPE, SIGBUS);

        SECTION("has") {
            REQUIRE(zero::os::process::ExitStatus{sig & 0x7f}.signal() == sig);
        }

        SECTION("no") {
            REQUIRE_FALSE(zero::os::process::ExitStatus{0}.signal());
        }
    }

    SECTION("stopped signal") {
        SECTION("has") {
            REQUIRE(zero::os::process::ExitStatus{(SIGSTOP << 8) | 0x7f}.stoppedSignal() == SIGSTOP);
        }

        SECTION("no") {
            REQUIRE_FALSE(zero::os::process::ExitStatus{0}.stoppedSignal());
        }
    }

    SECTION("core dumped") {
        const auto sig = GENERATE(SIGABRT, SIGSEGV, SIGILL, SIGFPE, SIGBUS);

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
    auto command = zero::os::process::Command{Program}
        .stdOutput(zero::os::process::Command::StdioType::Null);

    SECTION("add arg") {
        for (const auto &arg: Arguments)
            command.arg(arg);
    }

    SECTION("add args") {
        command.args({Arguments.begin(), Arguments.end()});
    }

    auto child = command.spawn();
    REQUIRE(child);
    Z_DEFER(child->wait());

    REQUIRE_THAT(zero::os::process::all(), Catch::Matchers::Contains(child->pid()));
}

#ifdef _WIN32
TEST_CASE("spawn child process with complex escape characters", "[os::process]") {
    constexpr std::array args{"\t", "\"", " ", "\\", "\t\", \\", "a", "b", "c"};

    auto child = zero::os::process::Command{"findstr"}
                    .args({args.begin(), args.end()})
                    .stdInput(zero::os::process::Command::StdioType::Null)
                    .stdOutput(zero::os::process::Command::StdioType::Null)
                    .stdError(zero::os::process::Command::StdioType::Null)
                    .spawn();
    REQUIRE(child);
    Z_DEFER(child->wait());

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
    const auto temp = zero::error::guard(zero::filesystem::canonical(zero::filesystem::temporaryDirectory()));

    auto child = zero::os::process::Command{Program}
                 .args({Arguments.begin(), Arguments.end()})
                 .currentDirectory(temp)
                 .stdOutput(zero::os::process::Command::StdioType::Null)
                 .spawn();
    REQUIRE(child);
    Z_DEFER(child->wait());
    REQUIRE(child->cwd() == temp);
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
    auto command = zero::os::process::Command{Program}
                   .args({Arguments.begin(), Arguments.end()})
                   .stdOutput(zero::os::process::Command::StdioType::Null);

    SECTION("default") {
        zero::env::set("ZERO_PROCESS_TESTS", "1");
        Z_DEFER(zero::env::unset("ZERO_PROCESS_TESTS"));

        auto child = command.spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(*envs | std::views::keys, Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
    }

    SECTION("add") {
        auto child = command.env("ZERO_PROCESS_TESTS", "1").spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(*envs | std::views::keys, Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
    }

    SECTION("remove") {
        zero::env::set("ZERO_PROCESS_TESTS", "1");
        Z_DEFER(zero::env::unset("ZERO_PROCESS_TESTS"));

        auto child = command.removeEnv("ZERO_PROCESS_TESTS").spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(*envs | std::views::keys, !Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
    }

    SECTION("clear") {
        zero::env::set("ZERO_PROCESS_TESTS", "1");
        Z_DEFER(zero::env::unset("ZERO_PROCESS_TESTS"));

        auto child = command.clearEnv().spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(*envs | std::views::keys, !Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
    }

    SECTION("set") {
        auto child = command.envs({{"ZERO_PROCESS_TESTS", "1"}}).spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto envs = child->envs();
        REQUIRE(envs);
        REQUIRE_THAT(*envs | std::views::keys, Catch::Matchers::Contains("ZERO_PROCESS_TESTS"));
        REQUIRE(envs->at("ZERO_PROCESS_TESTS") == "1");
    }
#endif
}

TEST_CASE("spawn child process with resource", "[os::process]") {
    using namespace std::chrono_literals;

    auto [reader, writer] = zero::os::pipe();

    SECTION("inherit") {
        auto duplicate = writer.duplicate(true);
        auto child = zero::os::process::Command{Program}
                     .args({Arguments.begin(), Arguments.end()})
                     .inheritedResource(zero::os::Resource{duplicate.release()})
                     .stdOutput(zero::os::process::Command::StdioType::Null)
                     .spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto tp = std::chrono::system_clock::now();
        zero::error::guard(writer.close());

        std::array<std::byte, 64> data{};
        REQUIRE(reader.read(data) == 0);
        REQUIRE(std::chrono::system_clock::now() - tp > 0.9s);
    }

    SECTION("without inherit") {
        auto child = zero::os::process::Command{Program}
                     .args({Arguments.begin(), Arguments.end()})
                     .stdOutput(zero::os::process::Command::StdioType::Null)
                     .spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto tp = std::chrono::system_clock::now();
        zero::error::guard(writer.close());

        std::array<std::byte, 64> data{};
        REQUIRE(reader.read(data) == 0);
        REQUIRE(std::chrono::system_clock::now() - tp < 0.9s);
    }
}

TEST_CASE("spawn child process with native resource", "[os::process]") {
    using namespace std::chrono_literals;

    auto [reader, writer] = zero::os::pipe();
    writer.setInheritable(true);

    SECTION("inherit") {
        auto child = zero::os::process::Command{Program}
                     .args({Arguments.begin(), Arguments.end()})
                     .inheritedNativeResource(writer.fd())
                     .stdOutput(zero::os::process::Command::StdioType::Null)
                     .spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto tp = std::chrono::system_clock::now();
        zero::error::guard(writer.close());

        std::array<std::byte, 64> data{};
        REQUIRE(reader.read(data) == 0);
        REQUIRE(std::chrono::system_clock::now() - tp > 0.9s);
    }

    SECTION("without inherit") {
        auto child = zero::os::process::Command{Program}
                     .args({Arguments.begin(), Arguments.end()})
                     .stdOutput(zero::os::process::Command::StdioType::Null)
                     .spawn();
        REQUIRE(child);
        Z_DEFER(child->wait());

        const auto tp = std::chrono::system_clock::now();
        zero::error::guard(writer.close());

        std::array<std::byte, 64> data{};
        REQUIRE(reader.read(data) == 0);
        REQUIRE(std::chrono::system_clock::now() - tp < 0.9s);
    }
}

TEST_CASE("spawn child process with piped stdio", "[os::process]") {
#ifdef _WIN32
    auto child = zero::os::process::Command{"findstr"}
                 .arg("hello")
                 .stdInput(zero::os::process::Command::StdioType::Piped)
                 .stdOutput(zero::os::process::Command::StdioType::Piped)
                 .spawn();
#else
    auto child = zero::os::process::Command{"cat"}
                 .stdInput(zero::os::process::Command::StdioType::Piped)
                 .stdOutput(zero::os::process::Command::StdioType::Piped)
                 .spawn();
#endif
    REQUIRE(child);
    Z_DEFER(child->wait());
    REQUIRE_FALSE(child->stdError());

    auto stdInput = std::exchange(child->stdInput(), std::nullopt);
    REQUIRE(stdInput);

    auto stdOutput = std::exchange(child->stdOutput(), std::nullopt);
    REQUIRE(stdOutput);

    constexpr std::string_view input{"hello world"};

    REQUIRE(stdInput->writeAll(std::as_bytes(std::span{input})));
    REQUIRE(stdInput->close());

    const auto output = stdOutput->readAll();
    REQUIRE(output);
    REQUIRE(zero::strings::trim({reinterpret_cast<const char *>(output->data()), output->size()}) == input);
}

TEST_CASE("spawn child process and collect status", "[os::process]") {
    const auto status = zero::os::process::Command{Program}
                        .args({Arguments.begin(), Arguments.end()})
                        .stdOutput(zero::os::process::Command::StdioType::Null)
                        .status();
    REQUIRE(status);
    REQUIRE(status->success());
}

TEST_CASE("spawn child process and collect output", "[os::process]") {
    const auto output = zero::os::process::Command{"hostname"}.output();
    REQUIRE(output);
    REQUIRE(output->status.success());

    REQUIRE(
        zero::strings::trim({reinterpret_cast<const char *>(output->out.data()),output->out.size()}) ==
        zero::os::hostname()
    );
}

TEST_CASE("spawn child process with pseudo console", "[os::process]") {
    using namespace std::string_view_literals;

    auto pc = zero::os::process::PseudoConsole::make(80, 32);
    REQUIRE(pc);

#ifdef _WIN32
    auto child = pc->spawn(zero::os::process::Command{"cmd"});
    REQUIRE(child);

    auto &[reader, writer] = pc->master();
    REQUIRE(reader);
    REQUIRE(writer);

    auto future = std::async([&] { return reader.readAll(); });
    REQUIRE(writer.writeAll(std::as_bytes(std::span{"echo hello\rexit\r"sv})));

    child->wait();
    pc->close();
#else
    auto child = pc->spawn(zero::os::process::Command{"sh"});
    REQUIRE(child);
    Z_DEFER(child->wait());

    auto &master = pc->master();
    REQUIRE(master);

    auto future = std::async([&] { return master.readAll(); });
    REQUIRE(master.writeAll(std::as_bytes(std::span{"echo hello\rexit\r"sv})));
#endif

    const auto data = future.get();
    REQUIRE(data);
    REQUIRE_THAT(
        (std::string{reinterpret_cast<const char *>(data->data()), data->size()}),
        Catch::Matchers::ContainsSubstring("hello")
    );
}
