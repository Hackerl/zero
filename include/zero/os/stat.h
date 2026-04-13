#ifndef ZERO_OS_STAT_H
#define ZERO_OS_STAT_H

#include <cstdint>

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

    CPUTime cpu();
    MemoryStat memory();
}

#endif //ZERO_OS_STAT_H
