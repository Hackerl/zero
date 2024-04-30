#include <zero/cmdline.h>
#include <zero/filesystem/path.h>
#include <cstring>
#include <fmt/std.h>

zero::Cmdline::Cmdline() : mOptionals{{"help", '?', "print help message", false}} {
}

// ReSharper disable once CppDFALocalValueEscapesFunction
zero::Optional &zero::Cmdline::find(const char shortName) {
    const auto it = ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.shortName == shortName;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(fmt::format("optional argument not exists[-{}]", shortName));

    return *it;
}

// ReSharper disable once CppDFALocalValueEscapesFunction
zero::Optional &zero::Cmdline::find(const std::string_view name) {
    const auto it = ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.name == name;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(fmt::format("optional argument not exists[--{}]", name));

    return *it;
}

// ReSharper disable once CppDFALocalValueEscapesFunction
const zero::Optional &zero::Cmdline::find(const std::string_view name) const {
    const auto it = ranges::find_if(
        mOptionals,
        [=](const auto &optional) {
            return optional.name == name;
        }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(fmt::format("optional argument not exists[--{}]", name));

    return *it;
}

void zero::Cmdline::help() const {
    fmt::print(
        stderr,
        "usage: {} [options] {} ... {} ...\n",
        filesystem::getApplicationPath()->filename(),
        fmt::join(
            mPositionals | ranges::views::transform([](const auto &p) {
                return fmt::format("{}({})", p.name, p.typeInfo.type);
            }),
            " "
        ),
        mFooter.value_or("extra")
    );

    fmt::print(stderr, "positional:\n");

    for (const auto &[name, desc, value, typeInfo]: mPositionals)
        fmt::print(stderr, "\t{:<30} {}({})\n", name, desc, typeInfo.type);

    fmt::print(stderr, "optional:\n");

    for (const auto &[name, shortName, desc, value, typeInfo]: mOptionals) {
        if (!shortName) {
            fmt::print(
                stderr,
                "\t    --{:<24} {}{}\n",
                name,
                desc,
                typeInfo ? fmt::format("({})", typeInfo->type) : ""
            );
            continue;
        }

        fmt::print(
            stderr,
            "\t-{}, --{:<24} {}{}\n",
            shortName,
            name,
            desc,
            typeInfo ? fmt::format("({})", typeInfo->type) : ""
        );
    }
}

void zero::Cmdline::addOptional(const char *name, const char shortName, const char *desc) {
    mOptionals.push_back({name, shortName, desc, false});
}

bool zero::Cmdline::exist(const char *name) const {
    return std::any_cast<bool>(find(name).value);
}

std::vector<std::string> zero::Cmdline::rest() const {
    return mRest;
}

void zero::Cmdline::footer(const char *message) {
    mFooter = message;
}

void zero::Cmdline::from(const int argc, const char *const *argv) {
    auto it = mPositionals.begin();

    for (int i = 1; i < argc; i++) {
        if (*argv[i] != '-') {
            if (it == mPositionals.end()) {
                mRest.emplace_back(argv[i]);
                continue;
            }

            auto value = it->typeInfo.parse(argv[i]);

            if (!value)
                throw std::runtime_error(fmt::format("invalid positional argument[{}]", argv[i]));

            it++->value = *std::move(value);
            continue;
        }

        if (argv[i][1] != '-') {
            auto &[name, shortName, desc, value, typeInfo] = find(argv[i][1]);

            if (!typeInfo) {
                value = true;
                continue;
            }

            if (!argv[i + 1])
                throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

            auto v = typeInfo->parse(argv[++i]);

            if (!v)
                throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

            value = *std::move(v);
            continue;
        }

        const char *p = strchr(argv[i], '=');
        const std::size_t n = p ? p - argv[i] : strlen(argv[i]);

        if (n == 2) {
            mRest.insert(mRest.end(), argv + i + 1, argv + argc);
            break;
        }

        auto &[name, shortName, desc, value, typeInfo] = find({argv[i] + 2, n - 2});

        if (!typeInfo) {
            value = true;
            continue;
        }

        if (!p)
            throw std::runtime_error(fmt::format("optional argument requires value[{}]", argv[i]));

        auto v = typeInfo->parse(p + 1);

        if (!v)
            throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

        value = *std::move(v);
    }

    if (exist("help")) {
        help();
        exit(EXIT_SUCCESS);
    }

    if (it != mPositionals.end())
        throw std::runtime_error(fmt::format("positional arguments not enough[{}]", it->name));
}

void zero::Cmdline::parse(const int argc, const char *const *argv) {
    try {
        from(argc, argv);
    }
    catch (const std::runtime_error &e) {
        fmt::print(stderr, "error:\n\t{}\n", e.what());
        help();
        exit(EXIT_FAILURE);
    }
}
