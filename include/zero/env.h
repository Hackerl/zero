#ifndef ZERO_ENV_H
#define ZERO_ENV_H

#include <map>
#include <optional>
#include <system_error>
#include <tl/expected.hpp>

namespace zero::env {
    tl::expected<std::optional<std::string>, std::error_code> get(const std::string &name);
    tl::expected<void, std::error_code> set(const std::string &name, const std::string &value);
    tl::expected<void, std::error_code> unset(const std::string &name);
    tl::expected<std::map<std::string, std::string>, std::error_code> list();
}

#endif //ZERO_ENV_H
