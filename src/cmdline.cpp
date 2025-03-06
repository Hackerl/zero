#include <zero/cmdline.h>
#include <zero/filesystem/fs.h>
#include <cassert>
#include <ranges>
#include <fmt/std.h>

zero::Cmdline::Cmdline() : mOptionals{{"help", '?', "print help message", false}} {
}

// ReSharper disable once CppDFALocalValueEscapesFunction
zero::Cmdline::Optional &zero::Cmdline::find(const char shortName) {
    const auto it = std::ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.shortName == shortName;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error{fmt::format("unknown optional argument '-{}'", shortName)};

    return *it;
}

// ReSharper disable once CppDFALocalValueEscapesFunction
zero::Cmdline::Optional &zero::Cmdline::find(const std::string_view name) {
    const auto it = std::ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.name == name;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error{fmt::format("unknown optional argument '--{}'", name)};

    return *it;
}

// ReSharper disable once CppDFALocalValueEscapesFunction
const zero::Cmdline::Optional &zero::Cmdline::find(const std::string_view name) const {
    const auto it = std::ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.name == name;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error{fmt::format("unknown optional argument '--{}'", name)};

    return *it;
}

void zero::Cmdline::help() const {
    fmt::print(
        stderr,
        "usage: {} [options] {} ... {} ...\n",
        filesystem::applicationPath()->filename(),
        fmt::join(
            mPositionals | std::views::transform([](const auto &positional) {
                return fmt::format("{}({})", positional.name, positional.typeInfo.name);
            }),
            " "
        ),
        mFooter.value_or("extra")
    );

    fmt::print(stderr, "positional:\n");

    for (const auto &[name, desc, value, typeInfo]: mPositionals)
        fmt::print(stderr, "\t{:<30} {}({})\n", name, desc, typeInfo.name);

    fmt::print(stderr, "optional:\n");

    for (const auto &[name, shortName, desc, value, typeInfo]: mOptionals) {
        if (!shortName) {
            fmt::print(
                stderr,
                "\t    --{:<24} {}{}\n",
                name,
                desc,
                typeInfo ? fmt::format("({})", typeInfo->name) : ""
            );
            continue;
        }

        fmt::print(
            stderr,
            "\t-{}, --{:<24} {}{}\n",
            shortName,
            name,
            desc,
            typeInfo ? fmt::format("({})", typeInfo->name) : ""
        );
    }
}

void zero::Cmdline::addOptional(std::string name, const char shortName, std::string desc) {
    mOptionals.emplace_back(std::move(name), shortName, std::move(desc), false);
}

bool zero::Cmdline::exist(const std::string_view name) const {
    const auto &optional = find(name);
    assert(!optional.typeInfo);
    return std::any_cast<bool>(optional.value);
}

std::vector<std::string> zero::Cmdline::rest() const {
    return mRest;
}

void zero::Cmdline::footer(std::string message) {
    mFooter = std::move(message);
}

void zero::Cmdline::parse(const std::span<const std::string_view> arguments) {
    auto it = mPositionals.begin();

    for (std::size_t i{0}; i < arguments.size(); ++i) {
        const auto &argument = arguments[i];

        // positional argument, may be a negative number
        if (argument.size() <= 1 || argument[0] != '-' || std::isdigit(static_cast<unsigned char>(argument[1]))) {
            if (it == mPositionals.end()) {
                mRest.emplace_back(argument);
                continue;
            }

            auto result = it->typeInfo.parse(argument);

            if (!result)
                throw std::runtime_error{
                    fmt::format(
                        "invalid value '{}' for positional argument '{}'[{} ({})]",
                        argument,
                        it->name,
                        result.error().message(),
                        result.error()
                    )
                };

            it++->value = *std::move(result);
            continue;
        }

        // short optional argument
        if (const auto c = argument[1]; c != '-') {
            auto &[name, shortName, desc, value, typeInfo] = find(c);

            if (!typeInfo) {
                value = true;
                continue;
            }

            if (i + 1 >= arguments.size())
                throw std::runtime_error{
                    fmt::format("optional argument '{}' requires a value but none was provided", argument)
                };

            auto result = typeInfo->parse(arguments[++i]);

            if (!result)
                throw std::runtime_error{
                    fmt::format(
                        "invalid value '{}' for optional argument '{}'[{} ({})]",
                        arguments[i],
                        argument,
                        result.error().message(),
                        result.error()
                    )
                };

            value = *std::move(result);
            continue;
        }

        if (argument.size() == 2) {
            std::ranges::transform(
                arguments.subspan(i + 1),
                std::back_inserter(mRest),
                [](const auto &arg) {
                    return std::string{arg};
                }
            );
            break;
        }

        // long optional argument
        const auto pos = argument.find('=');
        auto &[name, shortName, desc, value, typeInfo] = find(argument.substr(0, pos).substr(2));

        if (!typeInfo) {
            if (pos != std::string_view::npos)
                throw std::runtime_error{
                    fmt::format(
                        "unexpected value '{}' for flag '{}'",
                        argument.substr(pos + 1),
                        name
                    )
                };

            value = true;
            continue;
        }

        if (pos == std::string_view::npos || pos + 1 == argument.size())
            throw std::runtime_error{
                fmt::format(
                    "optional argument '{}' requires a value but none was provided",
                    argument.substr(0, pos)
                )
            };

        auto result = typeInfo->parse(argument.substr(pos + 1));

        if (!result)
            throw std::runtime_error{
                fmt::format(
                    "invalid value '{}' for optional argument '{}'[{} ({})]",
                    argument.substr(pos + 1),
                    argument.substr(0, pos),
                    result.error().message(),
                    result.error()
                )
            };

        value = *std::move(result);
    }

    if (exist("help")) {
        help();
        std::exit(EXIT_SUCCESS);
    }

    if (it != mPositionals.end())
        throw std::runtime_error{
            format(
                "missing required positional arguments: {}",
                fmt::join(
                    std::ranges::subrange{it, mPositionals.end()} | std::views::transform([](const auto &positional) {
                        return fmt::format("'{}'", positional.name);
                    }),
                    ", "
                )
            )
        };
}

void zero::Cmdline::parse(const int argc, const char *const *argv) {
    try {
        parse(std::vector<std::string_view>{argv + 1, argv + argc});
    }
    catch (const std::runtime_error &e) {
        fmt::print(stderr, "error:\n\t{}\n", e.what());
        help();
        std::exit(EXIT_FAILURE);
    }
}
