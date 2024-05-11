find_package(Catch2 CONFIG REQUIRED)

add_executable(
        zero_test
        log.cpp
        defer.cpp
        error.cpp
        expect.cpp
        cmdline.cpp
        formatter.cpp
        atomic/event.cpp
        atomic/circular_buffer.cpp
        concurrent/channel.cpp
        async/promise.cpp
        async/coroutine/error.cpp
        async/coroutine/exception.cpp
        filesystem/file.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        cache/lru.cpp
        os/os.cpp
        os/net.cpp
        os/stat.cpp
        os/process.cpp
        $<$<PLATFORM_ID:Windows>:os/nt/process.cpp>
        $<$<PLATFORM_ID:Darwin>:os/darwin/error.cpp os/darwin/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:os/procfs/process.cpp>
)

target_link_libraries(zero_test PRIVATE zero Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(zero_test)
