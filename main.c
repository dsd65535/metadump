#define _GNU_SOURCE

#include "common.h"
#include "statx-wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

int dev_major;
int dev_minor;

ssize_t llistxattr(
    const char *path,
    char *list,
    size_t size
) {
    return syscall(__NR_llistxattr, path, list, size);
}

ssize_t lgetxattr(
    const char *path,
    const char *name,
    void *value,
    size_t size
) {
    return syscall(__NR_lgetxattr, path, name, value, size);
}

void print_error(
    const char *filepath,
    const char *func,
    int ret
) {
    fprintf(
        stderr,
        "%s() failed with return code %i and errno %i for %s\n",
        func,
        ret,
        errno,
        filepath
    );
}

int dump_statx(
    const char *filepath,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx
) {
    int ret;

    memset(stx, 0xbf, sizeof(*stx));
    ret = statx(
        AT_FDCWD,
        filepath,
        AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC,
        STATX_ALL,
        stx
    );
    if (ret) {
        print_error(filepath, "statx", ret);
        return -1;
    }

    ret = fwrite(stx, sizeof(*stx), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(*stx);

    return 0;
}

int dump_ioctl(
    const char *filepath,
    FILE *datafile,
    int *datafile_pos,
    struct ioctl_data *ioc
) {
    int ret;
    int fd;

    fd = open(filepath, O_RDONLY | O_NONBLOCK | O_LARGEFILE | O_NOFOLLOW);

    if (fd < 0) {
        ret = fwrite(&errno, sizeof(errno), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(errno);
        return 0;
    }
    ret = fwrite(&NO_ERROR, sizeof(errno), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(errno);

    memset(ioc, 0x00, sizeof(*ioc));

    ioc->flags_ret = ioctl(fd, FS_IOC_GETFLAGS, &ioc->flags_buff);
    ioc->flags_errno = errno;

    ioc->version_ret = ioctl(fd, FS_IOC_GETVERSION, &ioc->version_buff);
    ioc->version_errno = errno;

    ioc->xattr_ret = ioctl(fd, FS_IOC_FSGETXATTR, &ioc->xattr_buff);
    ioc->xattr_errno = errno;

    ret = fwrite(ioc, sizeof(*ioc), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(*ioc);

    close(fd);

    return 0;
}

int dump_xattr(
    const char *filepath,
    FILE *datafile,
    int *datafile_pos
) {
    int ret;
    char *buff0;
    char *buff1;
    ssize_t length0;
    ssize_t length1;

    length0 = llistxattr(filepath, NULL, 0);

    ret = fwrite(&length0, sizeof(length0), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(length0);

    if (length0 < 1) {
        return 0;
    }

    buff0 = malloc(length0);
    if (buff0 == NULL) {
        fprintf(stderr, "malloc() failed with errno %i for %s\n", errno, filepath);
        return -1;
    }

    ret = llistxattr(filepath, buff0, length0);
    if (ret != length0) {
        print_error(filepath, "llistxattr", ret);
        return -1;
    }

    ret = fwrite(buff0, length0, 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += length0;

    for (char *name = buff0; name != buff0 + length0; name = strchr(name, '\0') + 1) {
        if (name[0] == '\0') {
            continue;
        }

        length1 = lgetxattr(filepath, name, NULL, 0);

        ret = fwrite(&length1, sizeof(length1), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(length1);

        if (length1 < 1) {
            continue;
        }

        buff1 = malloc(length1);
        if (buff1 == NULL) {
            fprintf(stderr, "malloc() failed with errno %i for %s\n", errno, filepath);
            return -1;
        }

        ret = lgetxattr(filepath, name, buff1, length1);
        if (ret != length1) {
            print_error(filepath, "lgetxattr", length1);
            return -1;
        }

        ret = fwrite(buff1, length1, 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += length1;

        free(buff1);
    }

    free(buff0);

    return 0;
}

int dump_dir(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx,
    struct ioctl_data *ioc
);

int dump_file(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx,
    struct ioctl_data *ioc,
    bool top_level
) {
    int ret;

    ret = dump_statx(filepath, datafile, datafile_pos, stx);
    if (ret) {
        return ret;
    }

    ret = dump_ioctl(filepath, datafile, datafile_pos, ioc);
    if (ret) {
        return ret;
    }

    ret = dump_xattr(filepath, datafile, datafile_pos);
    if (ret) {
        return ret;
    }

    if S_ISDIR(stx->stx_mode) {
        if (top_level) {
            dev_major = stx->stx_dev_major;
            dev_minor = stx->stx_dev_minor;
        } else {
            if (dev_major != stx->stx_dev_major || dev_minor != stx->stx_dev_minor) {
                fprintf(stderr, "skipping directory %s because it seems to be a mountpoint\n", filepath);
                return 0;
            }
        }
        ret = dump_dir(filepath, treefile, datafile, datafile_pos, stx, ioc);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

int dump_dir(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx,
    struct ioctl_data *ioc
) {
    int ret;
    struct dirent *de;
    DIR *dr;
    char *fullpath;

    ret = fwrite(&MARKER_START, sizeof(MARKER_START), 1, treefile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }

    dr = opendir(filepath);
    if (dr == NULL) {
        fprintf(stderr, "opendir() failed with errno %i for %s\n", errno, filepath);
        return -1;
    }

    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) {
            continue;
        }
        if (strcmp(de->d_name, "..") == 0) {
            continue;
        }

        ret = fwrite(datafile_pos, sizeof(*datafile_pos), 1, treefile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }

        ret = fwrite(de, sizeof(*de), 1, treefile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }

        fullpath = malloc(strlen(filepath)+ strlen(de->d_name) + 2);
        if (fullpath == NULL) {
            fprintf(stderr, "malloc() failed with errno %i for %s\n", errno, filepath);
            return -1;
        }

        sprintf(fullpath, "%s/%s", filepath, de->d_name);
        ret = dump_file(fullpath, treefile, datafile, datafile_pos, stx, ioc, false);
        if (ret) {
            return ret;
        }
        free(fullpath);
    }

    closedir(dr);

    ret = fwrite(&MARKER_END, sizeof(MARKER_END), 1, treefile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int ret;
    FILE *treefile = fopen(argv[1], "wb");
    FILE *datafile = fopen(argv[2], "wb");
    int datafile_pos = DATA_OFFSET;
    struct statx stx;
    struct ioctl_data ioc;

    if (argc != 4) {
        printf("Exactly 3 arguments required\n");
        return -1;
    }

    ret = fwrite(&VERSION, sizeof(VERSION), 1, treefile);
    if (ret != 1) {
        fprintf(
            stderr,
            "fwrite() failed with return code %i and errno %i\n",
            ret,
            errno
        );
        return -1;
    }
    ret = fwrite(&VERSION, sizeof(VERSION), 1, datafile);
    if (ret != 1) {
        fprintf(
            stderr,
            "fwrite() failed with return code %i and errno %i\n",
            ret,
            errno
        );
        return -1;
    }
    datafile_pos += sizeof(*&VERSION);

    ret = dump_file(argv[3], treefile, datafile, &datafile_pos, &stx, &ioc, true);
    if (ret) {
        return ret;
    }

    fclose(treefile);
    fclose(datafile);

    return 0;
}
