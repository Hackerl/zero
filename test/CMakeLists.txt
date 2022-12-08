include(FetchContent)

FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2
        GIT_TAG v3.2.0
)

FetchContent_MakeAvailable(catch2)

add_executable(zero_test strings/strings.cpp)
target_link_libraries(zero_test zero Catch2::Catch2WithMain)