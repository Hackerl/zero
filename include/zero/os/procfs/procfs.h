#ifndef ZERO_PROCFS_H
#define ZERO_PROCFS_H

#include <zero/error.h>

namespace zero::os::procfs {
    DEFINE_ERROR_CODE(
        Error,
        "zero::os::procfs",
        UNEXPECTED_DATA, "unexpected data"
    )
}

DECLARE_ERROR_CODE(zero::os::procfs::Error)

#endif //ZERO_PROCFS_H
