#ifndef METADUMP_STATX_WRAPPER_H
#define METADUMP_STATX_WRAPPER_H

#include <fcntl.h>
#include <unistd.h>
#include <linux/stat.h>
#include <sys/syscall.h>

#define statx foo
#define statx_timestamp foo_timestamp
struct statx;
struct statx_timestamp;
#include <sys/stat.h>
#undef statx
#undef statx_timestamp

#define AT_STATX_SYNC_TYPE    0x6000
#define AT_STATX_SYNC_AS_STAT 0x0000
#define AT_STATX_FORCE_SYNC   0x2000
#define AT_STATX_DONT_SYNC    0x4000

#ifndef __NR_statx
#define __NR_statx -1
#endif

ssize_t statx(int dfd, const char *filename, unsigned flags, unsigned int mask, struct statx *buffer);

#endif /* METADUMP_STATX_WRAPPER_H */
