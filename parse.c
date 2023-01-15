#include "common.h"
#include "statx-wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

void print_error(
    const char *func,
    int ret
) {
    fprintf(
        stderr,
        "%s() failed with return code %i and errno %i\n",
        func,
        ret,
        errno
    );
}

 int pop_parent(
    char **full_path,
    char **parent
) {
    char *slash_ptr;

    slash_ptr = strpbrk(*full_path, "/");
    if (slash_ptr == NULL) {
        *parent = *full_path;
    } else {
        *parent = malloc(slash_ptr - *full_path + 1);
        if (*parent == NULL) {
            fprintf(stderr, "malloc() failed with errno %i\n", errno);
            return -1;
        }
        memcpy(*parent, *full_path, slash_ptr - *full_path);
        (*parent)[slash_ptr - *full_path] = '\0';
        *full_path = slash_ptr + 1;
    };

    return 0;
 }

int find_file(
    FILE *treefile,
    char *search_path,
    int *marker
) {
    int ret;
    int level;
    char *target;
    int target_level;
    struct dirent de;

    target_level = 1;
    ret = pop_parent(&search_path, &target);
    if (ret) {
        return ret;
    }

    level = 0;
    for (;;) {
        ret = fread(marker, 1, sizeof(*marker), treefile);
        if (ret == 0) {
            fprintf(stderr, "File not found!\n");
            return -1;
        }
        if (ret != sizeof(*marker)) {
            print_error("fread", ret);
            return -1;
        }

        if (*marker == MARKER_START) {
            level++;
            continue;
        }
        if (*marker == MARKER_END) {
            level--;
            continue;
        }

        ret = fread(&de, sizeof(de), 1, treefile);
        if (ret != 1) {
            print_error("fread", ret);
            return -1;
        }

        if (level > target_level) {
            continue;
        }
        if (level < target_level) {
            fprintf(stderr, "File not found!\n");
            return -1;
        }

        ret = strcmp(target, de.d_name);
        if (ret == 0) {
            if (search_path == target) {
                break;
            }
            free(target);
            target_level++;
            ret = pop_parent(&search_path, &target);
            if (ret) {
                return ret;
            }
        }
    }

    printf("Directory Entry:\n");
    printf(" Inode: \t%u\n", de.d_ino);
    printf(" Offset:\t%u\n", de.d_off);
    printf(" Length:\t%u\n", de.d_reclen);
    printf(" Type:  \t%u\n", de.d_type);
    printf(" Name:  \t%s\n", de.d_name);

    return 0;
}

void check_statx_mask(
    int *stx_mask,
    int location
) {
    if (*stx_mask & location) {
        printf(" (valid)\n");
    } else {
        printf(" (invalid)\n");
    }
}

int parse_statx(
    FILE *datafile
) {
    int ret;
    struct statx stx;

    ret = fread(&stx, sizeof(stx), 1, datafile);
    if (ret != 1) {
        print_error("fread", ret);
        return -1;
    }
    printf("\nStatX:\n");

    printf(" Size:  \t%u", stx.stx_size);
    check_statx_mask(&stx.stx_mask, STATX_SIZE);
    printf(" Blocks:\t%u", stx.stx_blocks);
    check_statx_mask(&stx.stx_mask, STATX_BLOCKS);

    printf(" Inode: \t%u", stx.stx_ino);
    check_statx_mask(&stx.stx_mask, STATX_INO);
    printf(" Links: \t%u", stx.stx_nlink);
    check_statx_mask(&stx.stx_mask, STATX_NLINK);

    printf(" Type:  \t%o", stx.stx_mode & S_IFMT);
    check_statx_mask(&stx.stx_mask, STATX_TYPE);
    printf(" Mode:  \t%o", stx.stx_mode & ~S_IFMT);
    check_statx_mask(&stx.stx_mask, STATX_MODE);
    printf(" Uid:   \t%u", stx.stx_uid);
    check_statx_mask(&stx.stx_mask, STATX_UID);
    printf(" Gid:   \t%u", stx.stx_gid);
    check_statx_mask(&stx.stx_mask, STATX_GID);

    printf(" Access:\t%i.%09u", stx.stx_atime.tv_sec, stx.stx_atime.tv_nsec);
    check_statx_mask(&stx.stx_mask, STATX_ATIME);
    printf(" Modify:\t%i.%09u", stx.stx_mtime.tv_sec, stx.stx_mtime.tv_nsec);
    check_statx_mask(&stx.stx_mask, STATX_MTIME);
    printf(" Change:\t%i.%09u", stx.stx_ctime.tv_sec, stx.stx_ctime.tv_nsec);
    check_statx_mask(&stx.stx_mask, STATX_CTIME);
    printf(" Birth: \t%i.%09u", stx.stx_btime.tv_sec, stx.stx_btime.tv_nsec);
    check_statx_mask(&stx.stx_mask, STATX_BTIME);

    printf(" IO Block:\t%u\n", stx.stx_blksize);
    printf(" Device:\t%02x:%02x\n", stx.stx_dev_major, stx.stx_dev_minor);
    printf(" Rdev:  \t%02x:%02x\n", stx.stx_rdev_major, stx.stx_rdev_minor);

    printf(" Flags:  \t");
    for (int idx = 0; idx < 8 * sizeof(stx.stx_attributes_mask); idx++) {
        if (stx.stx_attributes_mask & (1 << idx)) {
            if (stx.stx_attributes & (1 << idx)) {
                printf("+");
            } else {
                printf("-");
            }
        } else {
            if (stx.stx_attributes & (1 << idx)) {
                printf("X");
            } else {
                printf(".");
            }
        }
    }
    printf("\n");

    return 0;
}

int main(
    int argc,
    char *argv[]
) {
    int ret;
    FILE *treefile;
    FILE *datafile;

    int version;
    int marker;

    if (argc != 4) {
        fprintf(stderr, "Exactly 3 arguments required\n");
        return -1;
    }

    treefile = fopen(argv[1], "rb");
    if (treefile == NULL) {
        fprintf(stderr, "Can't open treefile %s\n", argv[1]);
        return -1;
    }

    ret = fread(&version, sizeof(version), 1, treefile);
    if (ret != 1) {
        print_error("fread", ret);
        return -1;
    }
    if (version != VERSION) {
        fprintf(stderr, "Treefile version %i doesn't match current version %i\n", version, VERSION);
        return -1;
    }

    ret = find_file(treefile, argv[3], &marker);
    if (ret) {
        return ret;
    }

    fclose(treefile);

    datafile = fopen(argv[2], "rb");
    if (datafile == NULL) {
        fprintf(stderr, "Can't open datafile %s\n", argv[2]);
        return -1;
    }

    ret = fread(&version, sizeof(version), 1, datafile);
    if (ret != 1) {
        print_error("fread", ret);
        return -1;
    }
    if (version != VERSION) {
        fprintf(stderr, "Treefile version %i doesn't match current version %i\n", version, VERSION);
        return -1;
    }

    ret = fseek(datafile, marker - DATA_OFFSET, SEEK_SET);
    if (ret) {
        print_error("fseek", ret);
        return -1;
    }

    ret = parse_statx(datafile);
    if (ret) {
        return ret;
    }

    fclose(datafile);

    return 0;
}
