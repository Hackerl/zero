#include <zero/strings/strings.h>
#include <zero/defer.h>
#include <functional>
#include <iconv.h>
#include <range/v3/view.hpp>
#include <range/v3/algorithm.hpp>

bool zero::strings::startsWith(const std::string_view str, const std::string_view prefix) {
    if (str.length() < prefix.length())
        return false;

    return str.compare(0, prefix.length(), prefix) == 0;
}

bool zero::strings::endsWith(const std::string_view str, const std::string_view suffix) {
    if (str.length() < suffix.length())
        return false;

    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string zero::strings::trim(const std::string_view str) {
    return str
        | ranges::views::drop_while([](const unsigned char c) {
            return std::isspace(c);
        })
        | ranges::views::reverse
        | ranges::views::drop_while([](const unsigned char c) {
            return std::isspace(c);
        })
        | ranges::views::reverse
        | ranges::to<std::string>();
}

std::string zero::strings::ltrim(const std::string_view str) {
    return str
        | ranges::views::drop_while([](const unsigned char c) {
            return std::isspace(c);
        })
        | ranges::to<std::string>();
}

std::string zero::strings::rtrim(const std::string_view str) {
    return str
        | ranges::views::reverse
        | ranges::views::drop_while([](const unsigned char c) {
            return std::isspace(c);
        })
        | ranges::views::reverse
        | ranges::to<std::string>();
}

std::string zero::strings::tolower(const std::string_view str) {
    return str
        | ranges::views::transform([](const unsigned char c) {
            return std::tolower(c);
        })
        | ranges::to<std::string>();
}

std::string zero::strings::toupper(const std::string_view str) {
    return str
        | ranges::views::transform([](const unsigned char c) {
            return std::toupper(c);
        })
        | ranges::to<std::string>();
}

std::vector<std::string> zero::strings::split(const std::string_view str, int limit) {
    std::vector<std::string> tokens;

    auto prev = ranges::find_if(
        str,
        std::not_fn([](const unsigned char c) {
            return std::isspace(c);
        })
    );

    while (true) {
        if (prev == str.end())
            break;

        const auto it = ranges::find_if(
            prev,
            str.end(),
            [](const unsigned char c) {
                return std::isspace(c);
            }
        );

        if (it == str.end()) {
            tokens.emplace_back(prev, str.end());
            break;
        }

        tokens.emplace_back(prev, it);
        prev = ranges::find_if(
            it,
            str.end(),
            std::not_fn([](const unsigned char c) {
                return std::isspace(c);
            })
        );

        if (!--limit) {
            if (prev == str.end())
                break;

            tokens.emplace_back(prev, str.end());
            break;
        }
    }

    return tokens;
}

std::vector<std::string> zero::strings::split(const std::string_view str, const std::string_view delimiter, int limit) {
    std::vector<std::string> tokens;

    if (delimiter.empty()) {
        tokens.emplace_back(str);
        return tokens;
    }

    std::size_t prev{0};

    while (true) {
        const auto pos = str.find(delimiter, prev);

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

tl::expected<std::string, std::error_code>
zero::strings::encode(const std::wstring_view str, const std::string &encoding) {
    const auto cd = iconv_open(encoding.c_str(), "WCHAR_T");

    if (cd == reinterpret_cast<iconv_t>(-1))
        return tl::unexpected(std::error_code(errno, std::generic_category()));

    DEFER(iconv_close(cd));
    std::string output;

    auto input = reinterpret_cast<char *>(const_cast<wchar_t *>(str.data()));
    auto inBytesLeft = str.size() * sizeof(wchar_t);

    while (inBytesLeft > 0) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        auto ptr = buffer.data();
        auto outBytesLeft = buffer.size();

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG)
            return tl::unexpected(std::error_code(errno, std::generic_category()));

        output.append(buffer.data(), buffer.size() - outBytesLeft);
    }

    return output;
}

tl::expected<std::wstring, std::error_code>
zero::strings::decode(const std::string_view str, const std::string &encoding) {
    const auto cd = iconv_open("WCHAR_T", encoding.c_str());

    if (cd == reinterpret_cast<iconv_t>(-1))
        return tl::unexpected(std::error_code(errno, std::generic_category()));

    DEFER(iconv_close(cd));
    std::wstring output;

    auto input = const_cast<char *>(str.data());
    auto inBytesLeft = str.size();

    while (inBytesLeft > 0) {
        std::array<char, 1024> buffer; // NOLINT(*-pro-type-member-init)

        auto ptr = buffer.data();
        auto outBytesLeft = buffer.size();

        if (iconv(cd, &input, &inBytesLeft, &ptr, &outBytesLeft) == -1 && errno != E2BIG)
            return tl::unexpected(std::error_code(errno, std::generic_category()));

        output.append(reinterpret_cast<const wchar_t *>(buffer.data()), (buffer.size() - outBytesLeft) / sizeof(wchar_t));
    }

    return output;
}
