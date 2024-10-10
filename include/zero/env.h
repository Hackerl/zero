#ifndef ZERO_ENV_H
#define ZERO_ENV_H

#include <optional>
#include <expected>
#include <system_error>

namespace zero::env {
    std::expected<std::optional<std::string>, std::error_code> get(const std::string &name);
    std::expected<void, std::error_code> set(const std::string &name, const std::string &value);
    std::expected<void, std::error_code> unset(const std::string &name);
}

#endif //ZERO_ENV_H
