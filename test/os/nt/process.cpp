#include <zero/os/nt/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("windows process", "[process]") {
    auto ids = zero::os::nt::process::all();
    REQUIRE(ids);

    auto process = zero::os::nt::process::self();
    REQUIRE(process);
    REQUIRE(process->pid() == GetCurrentProcessId());

    auto path = zero::filesystem::getApplicationPath();
    REQUIRE(path);

    auto name = process->name();
    REQUIRE(name);
    REQUIRE(*name == path->filename());

    auto exe = process->exe();
    REQUIRE(exe);
    REQUIRE(*exe == *path);

    auto cmdline = process->cmdline();
    REQUIRE(cmdline);
    REQUIRE(cmdline->at(0).find(path->filename().string()) != std::string::npos);

    auto cwd = process->cwd();
    REQUIRE(cwd);
    REQUIRE(*cwd == std::filesystem::current_path());

    auto env = process->env();
    REQUIRE(env);

    auto memory = process->memory();
    REQUIRE(memory);

    auto cpu = process->cpu();
    REQUIRE(cpu);

    auto io = process->io();
    REQUIRE(io);
}
