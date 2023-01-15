#define _GNU_SOURCE

#include "common.h"
#include "statx-wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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

int dump_dir(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx
);

int dump_file(
    const char *filepath,
    FILE *treefile,
    FILE *datafile,
    int *datafile_pos,
    struct statx *stx
) {
    int ret;

    ret = dump_statx(filepath, datafile, datafile_pos, stx);
    if (ret) {
        return ret;
    }

    if S_ISDIR(stx->stx_mode) {
        ret = dump_dir(filepath, treefile, datafile, datafile_pos, stx);
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
    struct statx *stx
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
        ret = dump_file(fullpath, treefile, datafile, datafile_pos, stx);
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

    ret = dump_file(argv[3], treefile, datafile, &datafile_pos, &stx);
    if (ret) {
        return ret;
    }

    fclose(treefile);
    fclose(datafile);

    return 0;
}
