#include <zero/cmdline.h>
#include <iostream>
#include <cstring>

zero::Cmdline::Cmdline() : mOptionals({{"help", '?', "print help message", false}}) {

}

void zero::Cmdline::addOptional(const char *name, char shortName, const char *desc) {
    mOptionals.push_back(Optional{name, shortName, desc, false});
}

bool zero::Cmdline::exist(const char *name) {
    return std::any_cast<bool>(find(name).value);
}

std::string &zero::Cmdline::footer() {
    return mFooter;
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
                throw std::runtime_error(strings::format("invalid positional argument[%s]", argv[i]));

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
                throw std::runtime_error(strings::format("invalid optional argument[%s]", argv[i]));

            auto value = optional.typeInfo->parse(argv[++i]);

            if (!value)
                throw std::runtime_error(strings::format("invalid optional argument[%s]", argv[i]));

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
            throw std::runtime_error(strings::format("optional argument requires value[%s]", argv[i]));

        auto value = optional.typeInfo->parse(p + 1);

        if (!value)
            throw std::runtime_error(strings::format("invalid optional argument[%s]", argv[i]));

        optional.value = *value;
    }

    if (exist("help")) {
        help();
        exit(EXIT_SUCCESS);
    }

    if (it != mPositionals.end())
        throw std::runtime_error(strings::format("positional arguments not enough[%s]", it->name.c_str()));
}

void zero::Cmdline::parse(int argc, const char *const *argv) {
    try {
        from(argc, argv);
    } catch (const std::runtime_error &e) {
        std::cout << "error:" << std::endl;
        std::cout << '\t' << e.what() << std::endl;
        help();
        exit(EXIT_FAILURE);
    }
}

zero::Optional &zero::Cmdline::find(char shortName) {
    auto it = std::find_if(
            mOptionals.begin(),
            mOptionals.end(),
            [=](const auto &optional) {
                return optional.shortName == shortName;
            }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(strings::format("optional argument not exists[-%c]", shortName));

    return *it;
}

zero::Optional &zero::Cmdline::find(const std::string &name) {
    auto it = std::find_if(
            mOptionals.begin(),
            mOptionals.end(),
            [=](const auto &optional) {
                return optional.name == name;
            }
    );

    if (it == mOptionals.end())
        throw std::runtime_error(strings::format("optional argument not exists[--%s]", name.c_str()));

    return *it;
}

void zero::Cmdline::help() const {
    std::list<std::string> positionals;

    std::transform(
            mPositionals.begin(),
            mPositionals.end(),
            std::back_inserter(positionals),
            [](const auto &p) {
                return strings::format("%s(%s)", p.name.c_str(), p.typeInfo.type.c_str());
            }
    );

    std::cout << "usage: "
              << filesystem::getApplicationPath()->filename().string()
              << " [options] " << strings::join(positionals, " ")
              << " ... "
              << (mFooter.empty() ? "extra" : mFooter)
              << " ..."
              << std::endl;

    std::cout << "positional:" << std::endl;

    for (const auto &positional: mPositionals) {
        std::cout << '\t'
                  << std::left << std::setw(20) << positional.name
                  << positional.desc
                  << '('
                  << positional.typeInfo.type
                  << ')'
                  << std::endl;
    }

    std::cout << "optional:" << std::endl;

    for (const auto &optional: mOptionals) {
        std::cout << '\t';

        if (optional.shortName) {
            std::cout << '-' << optional.shortName << ", --";
        } else {
            std::cout << "    --";
        }

        std::cout << std::left << std::setw(14) << optional.name << optional.desc;

        if (optional.typeInfo)
            std::cout << '(' << optional.typeInfo->type << ')';

        std::cout << std::endl;
    }
}
