cmake_minimum_required(VERSION 3.17)
project(zero)

set(CMAKE_CXX_STANDARD 17)

option(ZERO_DISABLE_TESTS "disable unit test" OFF)
option(ZERO_DISABLE_LOG "disable log module" OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
        $<$<PLATFORM_ID:Windows>:src/os/process.cpp>
        $<$<PLATFORM_ID:Linux,Android>:src/os/procfs.cpp>
)

if (ZERO_DISABLE_LOG)
    target_compile_definitions(zero INTERFACE ZERO_NO_LOG)
endif ()

target_include_directories(
        zero
        PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
        zero
        INTERFACE
        $<$<PLATFORM_ID:Windows>:Ws2_32>
        $<$<PLATFORM_ID:Windows>:Iphlpapi>
        $<$<PLATFORM_ID:Windows>:Synchronization>
)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(EXCLUDE_HEADERS ${EXCLUDE_HEADERS} PATTERN "process.h" EXCLUDE)
endif ()

if (NOT CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    set(EXCLUDE_HEADERS ${EXCLUDE_HEADERS} PATTERN "procfs.h" EXCLUDE)
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

install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

if (NOT ZERO_DISABLE_TESTS)
    add_subdirectory(test)
endif ()