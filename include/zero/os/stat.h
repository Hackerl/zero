#ifndef ZERO_STAT_H
#define ZERO_STAT_H

#include <cstdint>
#include <system_error>
#include <tl/expected.hpp>

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

    tl::expected<CPUTime, std::error_code> cpu();
    tl::expected<MemoryStat, std::error_code> memory();
}

#endif //ZERO_STAT_H
