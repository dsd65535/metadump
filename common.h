#ifndef METADUMP_COMMON_H
#define METADUMP_COMMON_H

#include "statx-wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/fs.h>

#define MD_BUFF_SIZE 512

extern const int VERSION[3];

extern const int MARKER_START;
extern const int MARKER_END;
extern const int DATA_OFFSET;

extern const int NO_ERROR;

struct statx_data {
    int ret;
    int _errno;
    struct statx buff;
};

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

int update_buff(int new_length, int *old_length, char **buff);

#endif /* METADUMP_COMMON_H */
