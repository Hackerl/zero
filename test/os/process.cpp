#include <zero/os/process.h>
#include <zero/filesystem/path.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("process", "[process]") {
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

    const auto env = process->env();
    REQUIRE(env);

    const auto memory = process->memory();
    REQUIRE(memory);

    const auto cpu = process->cpu();
    REQUIRE(cpu);

    const auto io = process->io();
    REQUIRE(io);
}
