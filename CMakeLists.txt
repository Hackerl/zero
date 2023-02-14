cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 17)

option(ZERO_DISABLE_TESTS "disable unit test" OFF)
option(ZERO_DISABLE_LOG "disable log module" OFF)

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
        $<$<PLATFORM_ID:Linux,Android>:src/os/procfs.cpp>
)

if (ZERO_DISABLE_LOG)
    target_compile_definitions(zero INTERFACE ZERO_NO_LOG)
endif ()

target_include_directories(zero PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
        zero
        INTERFACE
        $<$<PLATFORM_ID:Windows>:Ws2_32>
        $<$<PLATFORM_ID:Windows>:Iphlpapi>
        $<$<PLATFORM_ID:Windows>:Synchronization>
)

if (NOT ZERO_DISABLE_TESTS)
    add_subdirectory(test)
endif ()