#ifndef ZERO_PROCFS_H
#define ZERO_PROCFS_H

#include <zero/error.h>

#undef linux

namespace zero::os::linux::procfs {
    DEFINE_ERROR_CODE(
        Error,
        "zero::os::linux::procfs",
        UNEXPECTED_DATA, "unexpected data"
    )
}

DECLARE_ERROR_CODE(zero::os::linux::procfs::Error)

#endif //ZERO_PROCFS_H
