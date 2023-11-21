#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include <pthread.h>

#include "string_parser.h"
#include "account.h"


int main(int argc, char* argv[]) {

    // check usage
    if (argc != 2) {
        printf("Usage: ./bank input.txt\n");
        return 1;
    }

    // declare line buffer
    size_t len = 0;
    char * line_buf = NULL;

    // open input file
    FILE * fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Unable to open input file");
        return 1;
    }

    // TESTING: print contents of file
    while (getline(&line_buf, &len, fp) != -1) {
        printf("%s", line_buf);
    }
    printf("\n");

    fclose(fp);
    free(line_buf);

    return 0;
}
