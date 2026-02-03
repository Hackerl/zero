#include <zero/utility.h>
#include <zero/error.h>

std::tm zero::localTime(const std::time_t time) {
    std::tm tm{};

#ifdef _WIN32
    if (const auto result = localtime_s(&tm, &time); result != 0)
        throw error::StacktraceError<std::system_error>{result, std::generic_category()};
#else
    if (!localtime_r(&time, &tm))
        throw error::StacktraceError<std::system_error>{errno, std::generic_category()};
#endif

    return tm;
}
