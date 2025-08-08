#include <zero/utility.h>
#include <system_error>

std::tm zero::localTime(const std::time_t time) {
    std::tm tm{};

#ifdef _WIN32
    if (const auto result = localtime_s(&tm, &time); result != 0)
        throw std::system_error{std::error_code{result, std::generic_category()}};
#else
    if (!localtime_r(&time, &tm))
        throw std::system_error{std::error_code{errno, std::generic_category()}};
#endif

    return tm;
}
