#include "common.h"

#include <stdio.h>
#include <errno.h>

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
    int marker;
    int level;

    char name[256];

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
        ret = fread(&marker, 1, sizeof(marker), treefile);
        if (ret == 0) {
            break;
        }
        if (ret != sizeof(marker)) {
            print_error("fread", ret);
            return -1;
        }

        if (marker == MARKER_START) {
            level++;
            continue;
        }
        if (marker == MARKER_END) {
            level--;
            continue;
        }

        ret = fread(&name, 19, 1, treefile);
        if (ret != 1) {
            print_error("fread", ret);
            return -1;
        }
        ret = fread(&name, 256, 1, treefile);
        if (ret != 1) {
            print_error("fread", ret);
            return -1;
        }

        for (int idx = 0; idx < level; idx++) {
            printf(" ");
        }
        printf("%s\n", name);

        ret = fread(&name, 5, 1, treefile);
        if (ret != 1) {
            print_error("fread", ret);
            return -1;
        }
    }

    fclose(treefile);

    return 0;
}
