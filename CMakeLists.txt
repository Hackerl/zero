cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

add_library(
        zero
        STATIC
        src/log.cpp
        src/os/os.cpp
        src/os/net.cpp
        src/os/process.cpp
        src/time/date.cpp
        src/strings/strings.cpp
        src/atomic/event.cpp
        src/filesystem/path.cpp
        src/encoding/base64.cpp
        src/encoding/hex.cpp
)

target_include_directories(zero PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
        zero
        $<$<BOOL:${WIN32}>:Ws2_32>
        $<$<BOOL:${WIN32}>:Iphlpapi>
        $<$<BOOL:${WIN32}>:Synchronization>
)