#ifndef ZERO_STRINGS_H
#define ZERO_STRINGS_H

#include <string>
#include <vector>
#include <numeric>
#include <memory>
#include <optional>
#include <charconv>

namespace zero::strings {
    bool containsIgnoreCase(std::string_view str, std::string_view substr);
    bool startsWith(std::string_view str, std::string_view prefix);
    bool endsWith(std::string_view str, std::string_view suffix);

    std::string trim(const std::string &str);
    std::string ltrim(std::string str);
    std::string rtrim(std::string str);
    std::string trimExtraSpace(std::string str);

    std::string tolower(std::string str);
    std::string toupper(std::string str);

    std::vector<std::string> split(std::string_view str, int limit = 0);
    std::vector<std::string> split(std::string_view str, std::string_view delimiter, int limit = 0);

    template<typename T>
    std::string join(const T &containers, const char *delimiter) {
        if (containers.empty())
            return "";

        return std::accumulate(
                std::next(containers.begin()),
                containers.end(),
                containers.front(),
                [=](auto result, const auto &value) {
                    return result + delimiter + value;
                }
        );
    }

    template<typename T>
    std::optional<T> toNumber(std::string_view str, int base = 10) {
        T value;

        std::from_chars_result result = std::from_chars(str.data(), str.data() + str.length(), value, base);

        if (result.ec != std::errc())
            return std::nullopt;

        return value;
    }

    template<typename... Args>
    std::string format(const char *fmt, Args... args) {
        int length = snprintf(nullptr, 0, fmt, args...);

        if (length <= 0)
            return "";

        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length + 1);
        snprintf(buffer.get(), length + 1, fmt, args...);

        return {buffer.get(), (std::size_t) length};
    }
}

#endif //ZERO_STRINGS_H
