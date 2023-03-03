#ifndef METADUMP_COMMON_H
#define METADUMP_COMMON_H

#include <stdio.h>
#include <linux/fs.h>

extern const int VERSION[3];

extern const int MARKER_START;
extern const int MARKER_END;
extern const int DATA_OFFSET;

extern const int NO_ERROR;

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

int compare_versions(int data_version[], const int parser_version[]);

#endif /* METADUMP_COMMON_H */
