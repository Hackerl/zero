#ifndef ZERO_ENV_H
#define ZERO_ENV_H

#include <map>
#include <string>
#include <optional>

namespace zero::env {
    std::optional<std::string> get(const std::string &name);
    void set(const std::string &name, const std::string &value);
    void unset(const std::string &name);
    std::map<std::string, std::string> list();
}

#endif //ZERO_ENV_H
