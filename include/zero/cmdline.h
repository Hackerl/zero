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

    struct COption {
        std::string name;
        std::string shortName;
        std::string desc;
        std::shared_ptr<IValue> value;
        bool flag = false;
    };

    class CArgParser {
    public:
        explicit CArgParser(const std::list<COption> &options) {
            mOptions = options;
        }

    public:
        template<typename T>
        T get(const std::string &name) {
            T v;
            COption option = findByName(name);

            if (!option.value->get(typeid(T).name(), &v)) {
                error(strings::format("get option failed: %s", name.c_str()));
            }

            return v;
        }

    public:
        void parse(int argc, char **argv) {
            for (const auto &option : mOptions) {
                auto it = std::find_if(
                        argv + 1,
                        argv + argc,
                        [&](const auto &argument) {
                            return (!option.name.empty() && strings::startsWith(argument, "--" + option.name)) ||
                                   (!option.shortName.empty() && strings::startsWith(argument, "-" + option.shortName));
                        }
                );

                if (it == argv + argc) {
                    continue;
                }

                if (option.flag) {
                    option.value->set("1");
                    continue;
                }

                if (!strings::startsWith(*it, "--")) {
                    if (!option.value->set(*(++it)))
                        error(strings::format("option invalid: %s", *it));

                    continue;
                }

                char *p = strchr(*it, '=');

                if (!p)
                    error(strings::format("invalid option: %s", *it));

                if (!option.value->set(p + 1))
                    error(strings::format("option invalid: %s", *it));
            }
        }

    private:
        COption findByName(const std::string &name) {
            auto it = std::find_if(
                    mOptions.begin(),
                    mOptions.end(),
                    [=](const auto &option) {
                        return option.name == name;
                    }
            );

            if (it == mOptions.end())
                error(strings::format("can't find option: %s", name.c_str()));

            return *it;
        }

    private:
        void help() {
            std::cout << "usage: ./" << filesystem::path::getApplicationName() << " [options]" << std::endl;
            exit(0);
        }

        void error(const std::string &message) {
            std::cout << message << std::endl;
            help();
        }

    private:
        std::list<COption> mOptions;
    };
}

#endif //ZERO_CMDLINE_H
