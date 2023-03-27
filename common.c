#include "common.h"

const int VERSION[] = {0, 2, 0};

const int MARKER_START = 0;
const int MARKER_END = 1;
const int DATA_OFFSET = 2;

const int NO_ERROR = 0;

int compare_versions(int data_version[], const int parser_version[]) {
    // Assume both versions consist of 3 integers

    if (data_version[0] != parser_version[0]) {
        printf("Major version mismatch\n");
        return -1;
    }

    if (data_version[0] == 0) {
        if (data_version[1] != parser_version[1]) {
            printf("Minor version mismatch\n");
            return -1;
        }
    } else {
        if (data_version[1] > parser_version[1]) {
            printf("Minor version outdated\n");
            return -1;
        }
    }

    if (data_version[1] == parser_version[1]) {
        if (data_version[2] > parser_version[2]) {
            printf("Patch version outdated\n");
            return -1;
        }
    }

    return 0;
}
