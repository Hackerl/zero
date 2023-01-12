cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 17)

option(ZERO_DISABLE_TESTS "disable unit test" OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

add_library(
        zero
        STATIC
        src/log.cpp
        src/os/os.cpp
        src/os/net.cpp
        src/time/date.cpp
        src/strings/strings.cpp
        src/atomic/event.cpp
        src/filesystem/path.cpp
        src/encoding/base64.cpp
        src/encoding/hex.cpp
        $<$<BOOL:$<PLATFORM_ID:Linux>>:src/os/procfs.cpp>
)

target_include_directories(zero PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
        zero
        $<$<BOOL:$<PLATFORM_ID:Windows>>:Ws2_32>
        $<$<BOOL:$<PLATFORM_ID:Windows>>:Iphlpapi>
        $<$<BOOL:$<PLATFORM_ID:Windows>>:Synchronization>
)

if (NOT ZERO_DISABLE_TESTS)
    add_subdirectory(test)
endif ()