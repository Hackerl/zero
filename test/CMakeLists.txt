find_package(Catch2 CONFIG REQUIRED)
find_package(FakeIt CONFIG REQUIRED)

add_executable(
        zero_test
        log.cpp
        env.cpp
        defer.cpp
        error.cpp
        expect.cpp
        utility.cpp
        cmdline.cpp
        formatter.cpp
        io/io.cpp
        io/binary.cpp
        io/buffer.cpp
        os/os.cpp
        os/net.cpp
        os/stat.cpp
        os/resource.cpp
        cache/lru.cpp
        async/promise.cpp
        atomic/event.cpp
        atomic/circular_buffer.cpp
        concurrent/channel.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        filesystem/fs.cpp
        meta/concepts.cpp
        meta/type_traits.cpp
        $<$<BOOL:${UNIX}>:os/unix/error.cpp>
        $<$<BOOL:${APPLE}>:os/macos/error.cpp>
        $<$<PLATFORM_ID:Windows>:os/windows/error.cpp os/windows/process.cpp>
        $<$<PLATFORM_ID:Darwin>:os/macos/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:os/linux/process.cpp os/linux/procfs/procfs.cpp os/linux/procfs/process.cpp>
        $<$<PLATFORM_ID:Windows,Darwin,Linux,Android>:os/process.cpp>
)

if (MSVC)
    target_compile_options(zero_test PRIVATE /utf-8)
endif ()

target_include_directories(zero_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
        zero_test
        PRIVATE
        zero
        Threads::Threads
        Catch2::Catch2WithMain
        FakeIt::FakeIt-catch
        $<$<PLATFORM_ID:Windows>:iphlpapi>
)

include(Catch)
catch_discover_tests(zero_test)
