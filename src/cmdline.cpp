#include <zero/cmdline.h>
#include <iostream>
#include <cstring>
#include <ranges>
#include <fmt/std.h>

zero::Cmdline::Cmdline() : mOptionals({{"help", '?', "print help message", false}}) {

}

void zero::Cmdline::addOptional(const char *name, char shortName, const char *desc) {
    mOptionals.emplace_back(name, shortName, desc, false);
}

bool zero::Cmdline::exist(const char *name) {
    return std::any_cast<bool>(find(name).value);
}

void zero::Cmdline::footer(const char *message) {
    mFooter = message;
}

std::vector<std::string> zero::Cmdline::rest() const {
    return mRest;
}

void zero::Cmdline::from(int argc, const char *const *argv) {
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

            it++->value = *value;
            continue;
        }

        if (argv[i][1] != '-') {
            auto &optional = find(argv[i][1]);

            if (!optional.typeInfo) {
                optional.value = true;
                continue;
            }

            if (!argv[i + 1])
                throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

            auto value = optional.typeInfo->parse(argv[++i]);

            if (!value)
                throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

            optional.value = *value;
            continue;
        }

        const char *p = strchr(argv[i], '=');
        size_t n = p ? p - argv[i] : strlen(argv[i]);

        if (n == 2) {
            mRest.insert(mRest.end(), argv + i + 1, argv + argc);
            break;
        }

        auto &optional = find({argv[i] + 2, n - 2});

        if (!optional.typeInfo) {
            optional.value = true;
            continue;
        }

        if (!p)
            throw std::runtime_error(fmt::format("optional argument requires value[{}]", argv[i]));

        auto value = optional.typeInfo->parse(p + 1);

        if (!value)
            throw std::runtime_error(fmt::format("invalid optional argument[{}]", argv[i]));

        optional.value = *value;
    }

    if (exist("help")) {
        help();
        exit(EXIT_SUCCESS);
    }

    if (it != mPositionals.end())
        throw std::runtime_error(fmt::format("positional arguments not enough[{}]", it->name));
}

void zero::Cmdline::parse(int argc, const char *const *argv) {
    try {
        from(argc, argv);
    } catch (const std::runtime_error &e) {
        fmt::print(stderr, "error:\n\t{}\n", e.what());
        help();
        exit(EXIT_FAILURE);
    }
}

zero::Optional &zero::Cmdline::find(char shortName) {
    auto it = std::ranges::find_if(
            mOptionals,
            [=](const auto &optional) {
                return optional.shortName == shortName;
            }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(fmt::format("optional argument not exists[-{}]", shortName));

    return *it;
}

zero::Optional &zero::Cmdline::find(const std::string &name) {
    auto it = std::ranges::find_if(
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
                    mPositionals | std::views::transform([](const auto &p) {
                        return fmt::format("{}({})", p.name, p.typeInfo.type);
                    }),
                    " "
            ),
            mFooter.value_or("extra")
    );

    fmt::print(stderr, "positional:\n");

    for (const auto &p: mPositionals)
        fmt::print(stderr, "\t{:<30} {}({})\n", p.name, p.desc, p.typeInfo.type);

    fmt::print(stderr, "optional:\n");

    for (const auto &p: mOptionals) {
        if (!p.shortName) {
            fmt::print(
                    stderr,
                    "\t    --{:<24} {}{}\n",
                    p.name,
                    p.desc,
                    p.typeInfo ? fmt::format("({})", p.typeInfo->type) : ""
            );
            continue;
        }

        fmt::print(
                stderr,
                "\t-{}, --{:<24} {}{}\n",
                p.shortName,
                p.name,
                p.desc,
                p.typeInfo ? fmt::format("({})", p.typeInfo->type) : ""
        );
    }
}
