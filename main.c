#define _GNU_SOURCE

#include "common.h"

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
#include <openssl/evp.h>

int dev_major;
int dev_minor;
struct statx_data stx;
struct ioctl_data ioc;
char *buff_llistxattr;
char *buff_lgetxattr;
int length_buff_llistxattr;
int length_buff_lgetxattr;

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
    int *datafile_pos
) {
    int ret;

    memset(&stx, 0x00, sizeof(stx));
    stx.ret = statx(
        AT_FDCWD,
        filepath,
        AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_FORCE_SYNC,
        STATX_ALL,
        &stx.buff
    );
    stx._errno = errno;
    if (stx.ret) {
        print_error(filepath, "statx", stx.ret);
    }

    ret = fwrite(&stx, sizeof(stx), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(stx);

    return 0;
}

int dump_ioctl_and_md5(
    const char *filepath,
    FILE *datafile,
    int *datafile_pos
) {
    int ret;
    int fd;

    EVP_MD_CTX *mdctx;
    char buff[MD_BUFF_SIZE];
    ssize_t bytes;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

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
        close(fd);
        return -1;
    }
    *datafile_pos += sizeof(errno);

    memset(&ioc, 0x00, sizeof(ioc));

    ioc.flags_ret = ioctl(fd, FS_IOC_GETFLAGS, &ioc.flags_buff);
    ioc.flags_errno = errno;

    ioc.version_ret = ioctl(fd, FS_IOC_GETVERSION, &ioc.version_buff);
    ioc.version_errno = errno;

    ioc.xattr_ret = ioctl(fd, FS_IOC_FSGETXATTR, &ioc.xattr_buff);
    ioc.xattr_errno = errno;

    ret = fwrite(&ioc, sizeof(ioc), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        close(fd);
        return -1;
    }
    *datafile_pos += sizeof(ioc);

    mdctx = EVP_MD_CTX_new();

    ret = EVP_DigestInit_ex2(mdctx, EVP_md5(), NULL);
    if (ret != 1) {
        print_error(filepath, "EVP_DigestInit_ex2", ret);
        EVP_MD_CTX_free(mdctx);
        close(fd);
        return -1;
    }

    bytes = read(fd, buff, sizeof(buff));
    while (bytes > 0) {
        ret = EVP_DigestUpdate(mdctx, buff, bytes);
        if (ret != 1) {
            print_error(filepath, "EVP_DigestUpdate", ret);
            EVP_MD_CTX_free(mdctx);
            close(fd);
            return -1;
        }
        bytes = read(fd, buff, sizeof(buff));
    }

    ret = EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    if (ret != 1) {
        print_error(filepath, "EVP_DigestFinal_ex", ret);
        EVP_MD_CTX_free(mdctx);
        close(fd);
        return -1;
    }

    ret = fwrite(&md_len, sizeof(md_len), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        close(fd);
        return -1;
    }
    *datafile_pos += sizeof(md_len);

    ret = fwrite(md_value, md_len, 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        close(fd);
        return -1;
    }
    *datafile_pos += md_len;

    EVP_MD_CTX_free(mdctx);
    close(fd);

    return 0;
}

int dump_xattr(
    const char *filepath,
    FILE *datafile,
    int *datafile_pos
) {
    int ret;
    ssize_t length_llistxattr;
    ssize_t length_lgetxattr;

    length_llistxattr = llistxattr(filepath, NULL, 0);

    ret = fwrite(&length_llistxattr, sizeof(length_llistxattr), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(length_llistxattr);

    if (length_llistxattr < 0) {
        ret = fwrite(&errno, sizeof(errno), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(errno);
    }
    if (length_llistxattr < 1) {
        return 0;
    }

    ret = update_buff(length_llistxattr, &length_buff_llistxattr, &buff_llistxattr);
    if (ret) {
        return ret;
    }

    length_llistxattr = llistxattr(filepath, buff_llistxattr, length_llistxattr);

    ret = fwrite(&length_llistxattr, sizeof(length_llistxattr), 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += sizeof(length_llistxattr);

    if (length_llistxattr < 0) {
        ret = fwrite(&errno, sizeof(errno), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(errno);
    }
    if (length_llistxattr < 1) {
        return 0;
    }

    ret = fwrite(buff_llistxattr, length_llistxattr, 1, datafile);
    if (ret != 1) {
        print_error(filepath, "fwrite", ret);
        return -1;
    }
    *datafile_pos += length_llistxattr;

    for (char *name = buff_llistxattr; name != buff_llistxattr + length_llistxattr; name = strchr(name, '\0') + 1) {
        if (name[0] == '\0') {
            continue;
        }

        length_lgetxattr = lgetxattr(filepath, name, NULL, 0);

        ret = fwrite(&length_lgetxattr, sizeof(length_lgetxattr), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(length_lgetxattr);

        if (length_lgetxattr < 0) {
            ret = fwrite(&errno, sizeof(errno), 1, datafile);
            if (ret != 1) {
                print_error(filepath, "fwrite", ret);
                return -1;
            }
            *datafile_pos += sizeof(errno);
        }
        if (length_lgetxattr < 1) {
            continue;
        }

        ret = update_buff(length_lgetxattr, &length_buff_lgetxattr, &buff_lgetxattr);
        if (ret) {
            return ret;
        }

        length_lgetxattr = lgetxattr(filepath, name, buff_lgetxattr, length_lgetxattr);

        ret = fwrite(&length_lgetxattr, sizeof(length_lgetxattr), 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += sizeof(length_lgetxattr);

        if (length_lgetxattr < 0) {
            ret = fwrite(&errno, sizeof(errno), 1, datafile);
            if (ret != 1) {
                print_error(filepath, "fwrite", ret);
                return -1;
            }
            *datafile_pos += sizeof(errno);
        }
        if (length_lgetxattr < 1) {
            continue;
        }

        ret = fwrite(buff_lgetxattr, length_lgetxattr, 1, datafile);
        if (ret != 1) {
            print_error(filepath, "fwrite", ret);
            return -1;
        }
        *datafile_pos += length_lgetxattr;
    }

    return 0;
}

int dump_dir(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos
);

int dump_file(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    bool top_level
) {
    int ret;

    ret = dump_statx(filepath, datafile, datafile_pos);
    if (ret) {
        return ret;
    }

    ret = dump_ioctl_and_md5(filepath, datafile, datafile_pos);
    if (ret) {
        return ret;
    }

    ret = dump_xattr(filepath, datafile, datafile_pos);
    if (ret) {
        return ret;
    }

    if (!stx.ret && S_ISDIR(stx.buff.stx_mode)) {
        if (top_level) {
            dev_major = stx.buff.stx_dev_major;
            dev_minor = stx.buff.stx_dev_minor;
        } else {
            if (dev_major != stx.buff.stx_dev_major || dev_minor != stx.buff.stx_dev_minor) {
                fprintf(stderr, "skipping directory %s because it seems to be a mountpoint\n", filepath);
                return 0;
            }
        }
        ret = dump_dir(filepath, treefile, datafile, datafile_pos);
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
    int *datafile_pos
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
        ret = dump_file(fullpath, treefile, datafile, datafile_pos, false);
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

    length_buff_llistxattr = 0;
    length_buff_lgetxattr = 0;
    buff_llistxattr = malloc(0);
    buff_lgetxattr = malloc(0);

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

    ret = dump_file(argv[3], treefile, datafile, &datafile_pos, true);
    if (ret) {
        return ret;
    }

    fclose(treefile);
    fclose(datafile);

    free(buff_llistxattr);
    free(buff_lgetxattr);

    return 0;
}
