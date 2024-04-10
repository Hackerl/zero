cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(ZERO_VERSION 1.0.3)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Iconv REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)
find_package(span-lite CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)

add_library(
        zero
        STATIC
        src/log.cpp
        src/cmdline.cpp
        src/os/os.cpp
        src/os/net.cpp
        src/os/stat.cpp
        src/os/process.cpp
        src/strings/strings.cpp
        src/atomic/event.cpp
        src/concurrent/channel.cpp
        src/filesystem/path.cpp
        src/filesystem/file.cpp
        src/encoding/base64.cpp
        src/encoding/hex.cpp
        $<$<PLATFORM_ID:Windows>:src/os/nt/process.cpp>
        $<$<PLATFORM_ID:Darwin>:src/os/darwin/error.cpp src/os/darwin/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:src/os/procfs/procfs.cpp src/os/procfs/process.cpp>
)

if (WIN32)
    target_compile_definitions(zero PUBLIC NOMINMAX)
endif ()

target_include_directories(
        zero
        PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
        zero
        PUBLIC
        Iconv::Iconv
        fmt::fmt
        tl::expected
        range-v3::meta
        range-v3::concepts
        range-v3::range-v3
        nonstd::span-lite
        INTERFACE
        Threads::Threads
        $<$<PLATFORM_ID:Windows>:Ws2_32>
        $<$<PLATFORM_ID:Windows>:Iphlpapi>
)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(EXCLUDE_HEADERS ${EXCLUDE_HEADERS} PATTERN "os/nt" EXCLUDE)
endif ()

if (NOT CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    set(EXCLUDE_HEADERS ${EXCLUDE_HEADERS} PATTERN "os/procfs.h" EXCLUDE)
endif ()

install(
        DIRECTORY
        include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        ${EXCLUDE_HEADERS}
)

install(
        TARGETS zero
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
        EXPORT ${PROJECT_NAME}Targets
        NAMESPACE zero::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

configure_package_config_file(
        cmake/${PROJECT_NAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        VERSION ${ZERO_VERSION}
        COMPATIBILITY SameMajorVersion
)

install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

if (BUILD_TESTING)
    add_subdirectory(test)
endif ()
