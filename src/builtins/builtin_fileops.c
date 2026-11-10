#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "builtins.h"

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0755)
#endif

int builtin_mkdir(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 2) {
        fprintf(stderr, "myshell: mkdir: missing operand\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (make_dir(argv[i]) != 0) {
            fprintf(stderr, "myshell: mkdir: cannot create directory '%s': %s\n",
                    argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}

int builtin_touch(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 2) {
        fprintf(stderr, "myshell: touch: missing file operand\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "ab+");
        if (!f) {
            fprintf(stderr, "myshell: touch: cannot touch '%s': %s\n",
                    argv[i], strerror(errno));
            status = 1;
        } else {
            fclose(f);
        }
    }
    return status;
}

