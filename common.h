#ifndef METADUMP_COMMON_H
#define METADUMP_COMMON_H

#include <linux/fs.h>

const int VERSION = 0;

const int MARKER_START = 0;
const int MARKER_END = 1;
const int DATA_OFFSET = 2;

const int NO_ERROR = 0;

struct ioctl_data {
    int flags_ret;
    int flags_errno;
    long flags_buff;
    int version_ret;
    int version_errno;
    long version_buff;
    int xattr_ret;
    int xattr_errno;
    struct fsxattr xattr_buff;
};

#endif /* METADUMP_COMMON_H */
