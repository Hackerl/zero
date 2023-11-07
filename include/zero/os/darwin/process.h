#ifndef ZERO_DARWIN_PROCESS_H
#define ZERO_DARWIN_PROCESS_H

#include <map>
#include <filesystem>
#include <tl/expected.hpp>

namespace zero::os::darwin::process {
    enum Error {
        UNEXPECTED_DATA = 1
    };

    class ErrorCategory : public std::error_category {
    public:
        [[nodiscard]] const char *name() const noexcept override;
        [[nodiscard]] std::string message(int value) const override;
    };

    const std::error_category &errorCategory();
    std::error_code make_error_code(Error e);

    struct CPUStat {
        double user;
        double system;
    };

    struct MemoryStat {
        uint64_t rss;
        uint64_t vms;
        uint64_t swap;
    };

    class Process {
    public:
        explicit Process(pid_t pid);

    public:
        [[nodiscard]] pid_t pid() const;
        [[nodiscard]] tl::expected<pid_t, std::error_code> ppid() const;

    public:
        [[nodiscard]] tl::expected<std::string, std::error_code> comm() const;
        [[nodiscard]] tl::expected<std::string, std::error_code> name() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> cwd() const;
        [[nodiscard]] tl::expected<std::filesystem::path, std::error_code> exe() const;
        [[nodiscard]] tl::expected<std::vector<std::string>, std::error_code> cmdline() const;
        [[nodiscard]] tl::expected<std::map<std::string, std::string>, std::error_code> environ() const;

    public:
        [[nodiscard]] tl::expected<CPUStat, std::error_code> cpu() const;
        [[nodiscard]] tl::expected<MemoryStat, std::error_code> memory() const;

    private:
        [[nodiscard]] tl::expected<std::vector<char>, std::error_code> arguments() const;

    private:
        pid_t mPID;
    };

    tl::expected<Process, std::error_code> open(pid_t pid);
}

namespace std {
    template<>
    struct is_error_code_enum<zero::os::darwin::process::Error> : public true_type {

    };
}

#endif //ZERO_DARWIN_PROCESS_H
