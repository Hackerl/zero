#ifndef ZERO_CMDLINE_H
#define ZERO_CMDLINE_H

#include "interface.h"
#include "filesystem/path.h"
#include "strings/string.h"
#include <string>
#include <memory>
#include <list>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <iomanip>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace zero {
    bool parseValue(const std::string &str, std::string &value) {
        value = str;
        return true;
    }

    template<typename T, std::enable_if_t<std::is_arithmetic<T>::value> * = nullptr>
    bool parseValue(const std::string &str, T &value) {
        return strings::toNumber(str, value);
    }

    template<typename T>
    bool parseValue(const std::string &str, std::vector<T> &value) {
        for (const auto &token : strings::split(str, ',')) {
            T v;

            if (!parseValue(strings::trim(token), v))
                return false;

            value.push_back(v);
        }

        return true;
    }

    template<typename T, std::enable_if_t<std::is_arithmetic<T>::value> * = nullptr>
    std::string demangle(const std::string &type) {
#ifdef _MSC_VER
        return type;
#elif __GNUC__
        int status = 0;
        std::unique_ptr<char, decltype(free) *> buffer(abi::__cxa_demangle(type.c_str(), nullptr, nullptr, &status), free);

        if (status != 0 || !buffer)
            return "unknown";

        return buffer.get();
#endif
    }

    template<typename T, std::enable_if_t<std::is_same<T, std::string>::value> * = nullptr>
    std::string demangle(const std::string &type) {
        return "string";
    }

    template<typename T, std::enable_if_t<std::is_same<T, std::vector<typename T::value_type, typename T::allocator_type>>::value> * = nullptr>
    std::string demangle(const std::string &type) {
        return strings::format("%s[]", demangle<typename T::value_type>(typeid(typename T::value_type).name()).c_str());
    }

    class IValue : public Interface {
    public:
        virtual bool get(const std::string &type, void *ptr) = 0;
        virtual bool set(const std::string &str) = 0;

    public:
        virtual std::string getType() = 0;
    };

    template<typename T>
    class Value : public IValue {
    public:
        template<typename... Args>
        explicit Value<T>(Args... args) : mValue(args...) {
            mType = typeid(T).name();
        }

    public:
        bool get(const std::string &type, void *ptr) override {
            if (type != mType)
                return false;

            *(T *)ptr = mValue;

            return true;
        }

        bool set(const std::string &str) override {
            return parseValue(str, mValue);
        }

    public:
        std::string getType() override {
            return demangle<T>(mType);
        }

    private:
        T mValue;
        std::string mType;
    };

    template<typename T, typename... Args>
    std::shared_ptr<IValue> value(Args... args) {
        return std::make_shared<Value<T>>(args...);
    }

    struct COptional {
        std::string name;
        char shortName;
        std::string desc;
        std::shared_ptr<IValue> value;
        bool flag = false;
    };

    struct CPositional {
        std::string name;
        std::string desc;
        std::shared_ptr<IValue> value;
    };

    class CCmdline {
    public:
        CCmdline() {
            addOptional({"help", '?', "print help message", value<bool>(), true});
        }

    public:
        void add(const CPositional &positional) {
            mPositionals.push_back(positional);
        }

        void addOptional(const COptional &optional) {
            mOptionals.push_back(optional);
        }

    public:
        template<typename T>
        T get(const std::string &name) {
            auto it = std::find_if(
                    mPositionals.begin(),
                    mPositionals.end(),
                    [=](const auto &positional) {
                        return positional.name == name;
                    }
            );

            if (it == mPositionals.end())
                error(strings::format("can't find positional argument: %s", name.c_str()));

            T v;

            if (!it->value->get(typeid(T).name(), &v)) {
                error(strings::format("get positional argument failed: %s", name.c_str()));
            }

            return v;
        }

        template<typename T>
        T getOptional(const std::string &name) {
            T v;
            COptional optional = findByName(name);

            if (!optional.value->get(typeid(T).name(), &v)) {
                error(strings::format("get optional argument failed: %s", name.c_str()));
            }

            return v;
        }

    public:
        void footer(const std::string &message) {
            mFooter = message;
        }

        std::vector<std::string> rest() {
            return mRest;
        }

    public:
        void parse(int argc, char **argv) {
            auto it = mPositionals.begin();

            for (int i = 1; i < argc; i++) {
                if (!strings::startsWith(argv[i], "-")) {
                    if (it == mPositionals.end()) {
                        mRest.emplace_back(argv[i]);
                        continue;
                    }

                    if (!it++->value->set(argv[i]))
                        error(strings::format("positional argument invalid: %s", argv[i]));

                    continue;
                }

                if (argv[i][1] != '-') {
                    COptional optional = findByShortName(argv[i][1]);

                    if (optional.flag) {
                        optional.value->set("1");
                        continue;
                    }

                    if (!argv[i + 1] || !optional.value->set(argv[++i]))
                        error(strings::format("optional short argument invalid: %s", argv[i]));

                    continue;
                }

                char *p = strchr(argv[i], '=');
                unsigned long n = p ? p - argv[i] : strlen(argv[i]);

                COptional optional = findByName({argv[i] + 2, n - 2});

                if (optional.flag) {
                    optional.value->set("1");
                    continue;
                }

                if (!p)
                    error(strings::format("optional argument need value: %s", argv[i]));

                if (!optional.value->set(p + 1))
                    error(strings::format("optional argument invalid: %s", argv[i]));
            }

            if (getOptional<bool>("help")) {
                help();
                exit(0);
            }

            if (it != mPositionals.end())
                error("positional arguments not enough");
        }

    private:
        COptional findByName(const std::string &name) {
            if (name.empty())
                error("require option name");

            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.name == name;
                    }
            );

            if (it == mOptionals.end())
                error(strings::format("can't find optional argument: %s", name.c_str()));

            return *it;
        }

        COptional findByShortName(char shortName) {
            if (!shortName)
                error("require option name");

            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.shortName == shortName;
                    }
            );

            if (it == mOptionals.end())
                error(strings::format("can't find optional short argument: %c", shortName));

            return *it;
        }

    private:
        void help() {
            std::list<std::string> positionals;

            std::transform(
                    mPositionals.begin(),
                    mPositionals.end(),
                    std::back_inserter(positionals),
                    [](const auto &p) {
                        return strings::format("%s(%s)", p.name.c_str(), p.value->getType().c_str());
                    }
            );

            std::cout << strings::format(
                    "usage: %s [options] %s ... %s ...",
                    filesystem::path::getApplicationName().c_str(),
                    strings::join(positionals, " ").c_str(),
                    mFooter.empty() ? "extra" : mFooter.c_str()
            ) << std::endl;

            std::cout << "positional:" << std::endl;

            for (const auto &positional : mPositionals) {
                std::cout << strings::format(
                        "\t%-20s%s(%s)",
                        positional.name.c_str(),
                        positional.desc.c_str(),
                        positional.value->getType().c_str()
                ) << std::endl;
            }

            std::cout << "optional:" << std::endl;

            for (const auto &optional : mOptionals) {
                std::cout << strings::format(
                        optional.shortName ? "\t-%c, --%-14s%s%s" : "\t%-4c--%-14s%s%s",
                        optional.shortName ? optional.shortName : ' ',
                        optional.name.c_str(),
                        optional.desc.c_str(),
                        optional.flag ? "" : strings::format("(%s)", optional.value->getType().c_str()).c_str()
                ) << std::endl;
            }
        }

        void error(const std::string &message) {
            std::cout << strings::format("error:\n\t%s", message.c_str()) << std::endl;
            help();
            exit(1);
        }

    private:
        std::string mFooter;
        std::vector<std::string> mRest;

    private:
        std::list<COptional> mOptionals;
        std::list<CPositional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
