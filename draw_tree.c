#include "common.h"

#include <stdio.h>
#include <errno.h>
#include <dirent.h>

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

int main(int argc, char *argv[]) {
    int ret;
    FILE *treefile;

    int version[3];
    int marker_or_datafile_pos;
    int level;

    struct dirent de;

    if (argc != 2) {
        fprintf(stderr, "Exactly 1 argument required\n");
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
    ret = compare_versions(version, VERSION);
    if (ret) {
        return ret;
    }

    level = 0;
    for (;;) {
        ret = fread(&marker_or_datafile_pos, 1, sizeof(marker_or_datafile_pos), treefile);
        if (ret == 0) {
            break;
        }
        if (ret != sizeof(marker_or_datafile_pos)) {
            print_error("fread", ret);
            return -1;
        }

        if (marker_or_datafile_pos == MARKER_START) {
            level++;
            continue;
        }
        if (marker_or_datafile_pos == MARKER_END) {
            level--;
            continue;
        }

        ret = fread(&de, sizeof(de), 1, treefile);
        if (ret != 1) {
            print_error("fread", ret);
            return -1;
        }

        for (int idx = 0; idx < level; idx++) {
            printf(" ");
        }
        printf("%s\n", de.d_name);
    }

    fclose(treefile);

    return 0;
}
