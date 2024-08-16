cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(ZERO_VERSION 1.0.8)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

if (BUILD_SHARED_LIBS AND MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Iconv REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_library(
        zero
        src/log.cpp
        src/cmdline.cpp
        src/os/os.cpp
        src/os/net.cpp
        src/os/stat.cpp
        src/os/process.cpp
        src/atomic/event.cpp
        src/concurrent/channel.cpp
        src/encoding/hex.cpp
        src/encoding/base64.cpp
        src/strings/strings.cpp
        src/filesystem/path.cpp
        src/filesystem/file.cpp
        $<$<PLATFORM_ID:Windows>:src/os/nt/error.cpp src/os/nt/process.cpp>
        $<$<PLATFORM_ID:Darwin>:src/os/darwin/error.cpp src/os/darwin/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:src/os/procfs/procfs.cpp src/os/procfs/process.cpp>
)

target_include_directories(
        zero
        PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
        zero
        PRIVATE
        Iconv::Iconv
        Threads::Threads
        $<$<PLATFORM_ID:Windows>:Ws2_32>
        $<$<PLATFORM_ID:Windows>:Iphlpapi>
        PUBLIC
        fmt::fmt
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(
            EXCLUDE_HEADERS
            ${EXCLUDE_HEADERS}
            PATTERN "os/unix" EXCLUDE
            PATTERN "os/darwin" EXCLUDE
            PATTERN "os/procfs" EXCLUDE
    )
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    set(
            EXCLUDE_HEADERS
            ${EXCLUDE_HEADERS}
            PATTERN "os/nt" EXCLUDE
            PATTERN "os/darwin" EXCLUDE
    )
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(
            EXCLUDE_HEADERS
            ${EXCLUDE_HEADERS}
            PATTERN "os/nt" EXCLUDE
            PATTERN "os/procfs" EXCLUDE
    )
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
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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
