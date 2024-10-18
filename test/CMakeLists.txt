find_package(Catch2 CONFIG REQUIRED)

add_executable(
        zero_test
        log.cpp
        env.cpp
        defer.cpp
        error.cpp
        expect.cpp
        cmdline.cpp
        formatter.cpp
        os/os.cpp
        os/net.cpp
        os/stat.cpp
        os/process.cpp
        cache/lru.cpp
        async/promise.cpp
        atomic/event.cpp
        atomic/circular_buffer.cpp
        concurrent/channel.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        filesystem/fs.cpp
        filesystem/std.cpp
        detail/type_traits.cpp
        $<$<BOOL:${UNIX}>:os/unix/error.cpp>
        $<$<PLATFORM_ID:Windows>:os/windows/error.cpp os/windows/process.cpp>
        $<$<PLATFORM_ID:Darwin>:os/macos/error.cpp os/macos/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:os/linux/process.cpp os/linux/procfs/procfs.cpp os/linux/procfs/process.cpp>
)

if (MSVC)
    target_compile_options(zero_test PRIVATE /utf-8)
endif ()

target_link_libraries(
        zero_test
        PRIVATE
        zero
        Threads::Threads
        Catch2::Catch2WithMain
        $<$<PLATFORM_ID:Windows>:Iphlpapi>
)

include(Catch)

# Catch2 v3.7.0 will inherit the PATH environment variable, so we don't need to set it manually.
catch_discover_tests(
        zero_test
        DL_PATHS
        $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${BUILD_SHARED_LIBS}>>:$ENV{PATH};$<TARGET_FILE_DIR:zero>>
)
