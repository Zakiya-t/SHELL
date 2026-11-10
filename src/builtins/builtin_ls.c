#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

static void format_permissions(mode_t mode, char *str)
{
    str[0] = S_ISDIR(mode) ? 'd' : '-';
    str[1] = (mode & 0400) ? 'r' : '-';
    str[2] = (mode & 0200) ? 'w' : '-';
    str[3] = (mode & 0100) ? 'x' : '-';
    str[4] = (mode & 0040) ? 'r' : '-';
    str[5] = (mode & 0020) ? 'w' : '-';
    str[6] = (mode & 0010) ? 'x' : '-';
    str[7] = (mode & 0004) ? 'r' : '-';
    str[8] = (mode & 0002) ? 'w' : '-';
    str[9] = (mode & 0001) ? 'x' : '-';
    str[10] = '\0';
}

static void format_size(off_t size, char *str, size_t max_len)
{
    if (size < 1024) {
        snprintf(str, max_len, "%5ld B", (long)size);
    } else if (size < 1024 * 1024) {
        snprintf(str, max_len, "%5.1f K", (double)size / 1024.0);
    } else {
        snprintf(str, max_len, "%5.1f M", (double)size / (1024.0 * 1024.0));
    }
}

int builtin_ls(int argc, char **argv, ShellState *state)
{
    (void)state;
    bool show_all = false;
    bool long_format = false;
    const char *target_dir = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; j < strlen(argv[i]); j++) {
                if (argv[i][j] == 'a')
                    show_all = true;
                else if (argv[i][j] == 'l')
                    long_format = true;
                else {
                    fprintf(stderr, "myshell: ls: unknown option '-%c'\n", argv[i][j]);
                    return 1;
                }
            }
        } else {
            target_dir = argv[i];
        }
    }

    DIR *dir = opendir(target_dir);
    if (!dir) {
        fprintf(stderr, "myshell: ls: %s: %s\n", target_dir, strerror(errno));
        return 1;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.')
            continue;

        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", target_dir, entry->d_name);

        struct stat st;
        bool has_stat = (stat(full_path, &st) == 0);

        bool is_dir = has_stat && S_ISDIR(st.st_mode);
        bool is_exec = has_stat && !is_dir && (st.st_mode & 0111);

        if (long_format) {
            char perm[12] = "----------";
            char size_str[16] = "       ";
            char time_str[32] = "";

            if (has_stat) {
                format_permissions(st.st_mode, perm);
                format_size(st.st_size, size_str, sizeof(size_str));
                struct tm *tm_info = localtime(&st.st_mtime);
                if (tm_info) {
                    strftime(time_str, sizeof(time_str), "%b %d %H:%M", tm_info);
                }
            }

            printf("%s%s%s  %s  %s  ", COLOR_DIM, perm, COLOR_RESET, size_str, time_str);

            if (is_dir) {
                printf("%s%s%s/%s\n", COLOR_BOLD, COLOR_BRIGHT_BLUE, entry->d_name, COLOR_RESET);
            } else if (is_exec) {
                printf("%s%s%s*%s\n", COLOR_BOLD, COLOR_BRIGHT_GREEN, entry->d_name, COLOR_RESET);
            } else {
                printf("%s\n", entry->d_name);
            }
        } else {
            /* Compact grid format with colors */
            if (is_dir) {
                printf("%s%s%s/%s", COLOR_BOLD, COLOR_BRIGHT_BLUE, entry->d_name, COLOR_RESET);
            } else if (is_exec) {
                printf("%s%s%s*%s", COLOR_BOLD, COLOR_BRIGHT_GREEN, entry->d_name, COLOR_RESET);
            } else {
                printf("%s", entry->d_name);
            }

            count++;
            printf("%s", (count % 4 == 0) ? "\n" : "\t");
        }
    }

    if (!long_format && count % 4 != 0) {
        putchar('\n');
    }

    closedir(dir);
    return 0;
}

