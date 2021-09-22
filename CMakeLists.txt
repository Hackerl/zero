cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

add_library(
        zero
        STATIC
        src/log.cpp
        src/time/date.cpp
        src/shell/shell.cpp
        src/strings/str.cpp
        src/atomic/event.cpp
        src/filesystem/path.cpp
)

target_include_directories(zero PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)