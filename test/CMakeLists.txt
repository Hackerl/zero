find_package(Catch2 CONFIG REQUIRED)

add_executable(
        zero_test
        try.cpp
        log.cpp
        defer.cpp
        cmdline.cpp
        ptr/ref.cpp
        atomic/event.cpp
        atomic/circular_buffer.cpp
        concurrent/channel.cpp
        async/promise.cpp
        async/coroutine/error.cpp
        async/coroutine/exception.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        cache/lru.cpp
        os/os.cpp
        os/net.cpp
        os/process.cpp
        $<$<PLATFORM_ID:Windows>:os/nt/process.cpp>
        $<$<PLATFORM_ID:Darwin>:os/darwin/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:os/procfs.cpp>
)

target_link_libraries(zero_test PRIVATE zero Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(zero_test)