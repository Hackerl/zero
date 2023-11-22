#include <zero/strings/strings.h>
#include <zero/defer.h>
#include <ranges>
#include <functional>
#include <iconv.h>

std::string zero::strings::trim(std::string_view str) {
    auto v = str
             | std::views::drop_while(isspace)
             | std::views::reverse
             | std::views::drop_while(isspace)
             | std::views::reverse;

    return {v.begin(), v.end()};
}

std::string zero::strings::ltrim(std::string_view str) {
    auto v = str
             | std::views::drop_while(isspace);

    return {v.begin(), v.end()};
}

std::string zero::strings::rtrim(std::string_view str) {
    auto v = str
             | std::views::reverse
             | std::views::drop_while(isspace)
             | std::views::reverse;

    return {v.begin(), v.end()};
}

std::string zero::strings::tolower(std::string_view str) {
    const auto v = str
             | std::views::transform(::tolower);

    return {v.begin(), v.end()};
}

std::string zero::strings::toupper(std::string_view str) {
    const auto v = str
             | std::views::transform(::toupper);

    return {v.begin(), v.end()};
}

std::vector<std::string> zero::strings::split(std::string_view str, int limit) {
    std::vector<std::string> tokens;
    auto prev = std::ranges::find_if(str, std::not_fn(isspace));

    while (true) {
        if (prev == str.end())
            break;

        const auto it = std::ranges::find_if(prev, str.end(), isspace);

        if (it == str.end()) {
            tokens.emplace_back(prev, str.end());
            break;
        }

        tokens.emplace_back(prev, it);
        prev = std::ranges::find_if(it, str.end(), std::not_fn(isspace));

        if (!--limit) {
            if (prev == str.end())
                break;

            tokens.emplace_back(prev, str.end());
            break;
        }
    }

    return tokens;
}

std::vector<std::string> zero::strings::split(std::string_view str, const std::string_view delimiter, int limit) {
    std::vector<std::string> tokens;

    if (delimiter.empty()) {
        tokens.emplace_back(str);
        return tokens;
    }

    std::size_t prev = 0;

    while (true) {
        const std::size_t pos = str.find(delimiter, prev);

        if (pos == std::string::npos) {
            tokens.emplace_back(str.substr(prev));
            break;
        }

        tokens.emplace_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.length();

        if (!--limit) {
            tokens.emplace_back(str.substr(prev));
            break;
        }
    }

    return tokens;
}

tl::expected<std::string, std::error_code> zero::strings::encode(std::wstring_view str, const char *encoding) {
    const iconv_t cd = iconv_open(encoding, "WCHAR_T");

    if (cd == reinterpret_cast<iconv_t>(-1))
        return tl::unexpected(std::error_code(errno, std::system_category()));

    DEFER(iconv_close(cd));
    tl::expected<std::string, std::error_code> output;

    auto input = reinterpret_cast<char *>(const_cast<wchar_t *>(str.data()));
    std::size_t inBytesLeft = str.length() * sizeof(wchar_t);

    while (inBytesLeft > 0) {
        char buffer[1024] = {};

        auto ptr = buffer;
        std::size_t outBytesLeft = sizeof(buffer);

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG) {
            output = tl::unexpected(std::error_code(errno, std::system_category()));
            break;
        }

        output->append(buffer, sizeof(buffer) - outBytesLeft);
    }

    return output;
}

tl::expected<std::wstring, std::error_code> zero::strings::decode(std::string_view str, const char *encoding) {
    const iconv_t cd = iconv_open("WCHAR_T", encoding);

    if (cd == reinterpret_cast<iconv_t>(-1))
        return tl::unexpected(std::error_code(errno, std::system_category()));

    DEFER(iconv_close(cd));
    tl::expected<std::wstring, std::error_code> output;

    auto input = const_cast<char *>(str.data());
    std::size_t inBytesLeft = str.length();

    while (inBytesLeft > 0) {
        char buffer[1024] = {};

        auto ptr = buffer;
        std::size_t outBytesLeft = sizeof(buffer);

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG) {
            output = tl::unexpected(std::error_code(errno, std::system_category()));
            break;
        }

        output->append(reinterpret_cast<wchar_t *>(buffer), (sizeof(buffer) - outBytesLeft) / sizeof(wchar_t));
    }

    return output;
}
