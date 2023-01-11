find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2
        GIT_TAG v3.2.0
)

FetchContent_MakeAvailable(catch2)

add_executable(
        zero_test
        atomic/event.cpp
        atomic/circular_buffer.cpp
        async/promise.cpp
        encoding/hex.cpp
        encoding/base64.cpp
        strings/strings.cpp
        cache/lru.cpp
        $<$<BOOL:$<PLATFORM_ID:Linux>>:os/process.cpp>
)

target_link_libraries(zero_test zero Catch2::Catch2WithMain Threads::Threads)