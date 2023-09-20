#ifndef ZERO_STRINGS_H
#define ZERO_STRINGS_H

#include <string>
#include <vector>
#include <numeric>
#include <memory>
#include <charconv>
#include <stdexcept>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::strings {
    bool startsWith(std::string_view str, std::string_view prefix);
    bool endsWith(std::string_view str, std::string_view suffix);

    std::string trim(const std::string &str);
    std::string ltrim(std::string str);
    std::string rtrim(std::string str);

    std::string tolower(std::string str);
    std::string toupper(std::string str);

    std::vector<std::string> split(std::string_view str, int limit = 0);
    std::vector<std::string> split(std::string_view str, std::string_view delimiter, int limit = 0);

    tl::expected<std::string, std::error_code> encode(const std::wstring &str, const char *encoding = "UTF-8");
    tl::expected<std::wstring, std::error_code> decode(const std::string &str, const char *encoding = "UTF-8");

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
    tl::expected<T, std::error_code> toNumber(std::string_view str, int base = 10) {
        T value;

        std::from_chars_result result = std::from_chars(str.data(), str.data() + str.length(), value, base);

        if (result.ec != std::errc())
            return tl::unexpected(make_error_code(result.ec));

        return value;
    }

    template<typename... Args>
    std::string format(const char *fmt, Args... args) {
        size_t length = 4096;
        auto buffer = std::make_unique<char[]>(length);

        while (true) {
            int n = snprintf(buffer.get(), length, fmt, args...);

            if (n < 0)
                throw std::invalid_argument("format string error");

            if (n >= length) {
                length = n + 1;
                buffer = std::make_unique<char[]>(length);
                continue;
            }

            break;
        }

        return buffer.get();
    }
}

#endif //ZERO_STRINGS_H
