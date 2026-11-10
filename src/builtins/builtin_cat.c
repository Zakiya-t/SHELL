#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

int builtin_cat(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 2) {
        fprintf(stderr, "myshell: cat: missing file operand\n");
        fprintf(stderr, "usage: cat [-n] <file...>\n");
        return 1;
    }

    bool number_lines = false;
    int file_start = 1;

    if (strcmp(argv[1], "-n") == 0) {
        number_lines = true;
        file_start = 2;
    }

    if (file_start >= argc) {
        fprintf(stderr, "myshell: cat: missing file operand after -n\n");
        return 1;
    }

    int overall_status = 0;
    int line_number = 1;

    for (int i = file_start; i < argc; i++) {
        FILE *file = fopen(argv[i], "r");
        if (!file) {
            fprintf(stderr, "myshell: cat: %s: %s\n", argv[i], strerror(errno));
            overall_status = 1;
            continue;
        }

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), file)) {
            if (number_lines) {
                printf("%s%6d  %s%s", COLOR_DIM, line_number++, COLOR_RESET, buffer);
            } else {
                fputs(buffer, stdout);
            }
        }

        fclose(file);
    }

    fflush(stdout);
    return overall_status;
}

