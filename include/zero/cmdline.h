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
#include <cxxabi.h>

namespace zero {
    bool parseValue(const std::string &str, std::string &value) {
        value = str;
        return true;
    }

    template<typename T, std::enable_if_t<std::is_arithmetic<T>::value> * = nullptr>
    bool parseValue(const std::string &str, T &value) {
        return strings::toNumber(str, value);
    }

    template <typename T>
    bool parseValue(const std::string &str, std::vector<T> &value) {
        for (const auto &token : strings::split(str, ',')) {
            T v;

            if (!parseValue(strings::trim(token), v))
                return false;

            value.push_back(v);
        }

        return true;
    }

    template <typename T>
    std::string demangle(const std::string &type) {
        int status = 0;
        std::unique_ptr<char> buffer(abi::__cxa_demangle(type.c_str(), nullptr, nullptr, &status));

        if (status != 0 || !buffer)
            return "unknown";

        return {buffer.get()};
    }

    template <>
    std::string demangle<std::string>(const std::string &type) {
        return "string";
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

    template <typename T, typename... Args>
    std::shared_ptr<IValue> value(Args... args) {
        return std::make_shared<Value<T>>(args...);
    }

    struct COptional {
        std::string name;
        std::string shortName;
        std::string desc;
        std::shared_ptr<IValue> value;
        bool flag = false;
    };

    struct CPositional {
        std::string name;
        std::string desc;
        std::shared_ptr<IValue> value;
    };

    class CArgParser {
    public:
        CArgParser() {
            addOptional({"help", "?", "print help message", value<bool>(), true});
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
            auto it = std::find_if(
                    mOptionals.begin(),
                    mOptionals.end(),
                    [=](const auto &optional) {
                        return optional.name == name;
                    }
            );

            if (it == mOptionals.end())
                error(strings::format("can't find optional argument: %s", name.c_str()));

            T v;

            if (!it->value->get(typeid(T).name(), &v)) {
                error(strings::format("get optional argument failed: %s", name.c_str()));
            }

            return v;
        }

    public:
        void parse(int argc, char **argv) {
            for (const auto &optional : mOptionals) {
                auto it = std::find_if(
                        argv + 1,
                        argv + argc,
                        [&](const auto &argument) {
                            return (!optional.name.empty() && strings::startsWith(argument, "--" + optional.name)) ||
                                   (!optional.shortName.empty() && strings::startsWith(argument, "-" + optional.shortName));
                        }
                );

                if (it == argv + argc) {
                    continue;
                }

                if (optional.flag) {
                    optional.value->set("1");
                    continue;
                }

                if (!strings::startsWith(*it, "--")) {
                    if (!*(++it) || !optional.value->set(*it))
                        error(strings::format("optional argument invalid: %s", optional.name.c_str()));

                    continue;
                }

                char *p = strchr(*it, '=');

                if (!p)
                    error(strings::format("optional argument invalid: %s", *it));

                if (!optional.value->set(p + 1))
                    error(strings::format("optional argument invalid: %s", *it));
            }

            if (getOptional<bool>("help"))
                help();

            if (mPositionals.empty())
                return;

            char **p = argv + 1;

            for (const auto &positional : mPositionals) {
                while (strings::startsWith(*p, "-")) {
                    if (p + 1 == argv + argc)
                        error(strings::format("need positional argument: %s", positional.name.c_str()));

                    if (*p[1] == '-') {
                        p++;
                        continue;
                    }

                    bool flag = std::any_of(
                            mOptionals.begin(),
                            mOptionals.end(),
                            [=](const auto &optional) {
                                return optional.shortName == *p + 1 && optional.flag;
                            });

                    p += flag ? 1 : 2;
                }

                if (!positional.value->set(*p))
                    error(strings::format("positional argument invalid: %s", *p));
            }
        }

    private:
        void help() {
            exit(0);
        }

        void error(const std::string &message) {
            std::cout << message << std::endl;
            help();
        }

    private:
        std::list<COptional> mOptionals;
        std::list<CPositional> mPositionals;
    };
}

#endif //ZERO_CMDLINE_H
