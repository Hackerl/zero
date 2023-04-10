find_package(Threads REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(
        zero_test
        log.cpp
        cmdline.cpp
        atomic/event.cpp
        atomic/circular_buffer.cpp
        async/promise.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        cache/lru.cpp
        $<$<PLATFORM_ID:Windows>:os/nt/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:os/procfs.cpp>
)

target_link_libraries(zero_test PRIVATE zero Catch2::Catch2WithMain Threads::Threads)