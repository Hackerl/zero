#ifndef ZERO_STAT_H
#define ZERO_STAT_H

#include <cstdint>
#include <expected>
#include <system_error>

namespace zero::os::stat {
    struct CPUTime {
        double user{};
        double system{};
        double idle{};
    };

    struct MemoryStat {
        std::uint64_t total{};
        std::uint64_t used{};
        std::uint64_t available{};
        std::uint64_t free{};
        double usedPercent{};
    };

    std::expected<CPUTime, std::error_code> cpu();
    std::expected<MemoryStat, std::error_code> memory();
}

#endif //ZERO_STAT_H
