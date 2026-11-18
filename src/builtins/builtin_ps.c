#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#endif

typedef struct {
    int pid;
    int ppid;
    char user[32];
    char state;
    char time_str[16];
    char cmd[256];
} ProcessRecord;

#ifndef _WIN32
static void get_process_user(int pid, char *user_dest, size_t max_len)
{
    strncpy(user_dest, "unknown", max_len);
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    FILE *f = fopen(status_path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            uid_t uid = 0;
            if (sscanf(line + 4, "%u", &uid) == 1) {
                struct passwd *pw = getpwuid(uid);
                if (pw && pw->pw_name) {
                    strncpy(user_dest, pw->pw_name, max_len - 1);
                    user_dest[max_len - 1] = '\0';
                } else {
                    snprintf(user_dest, max_len, "%u", uid);
                }
            }
            break;
        }
    }
    fclose(f);
}

static void get_process_cmdline(int pid, const char *comm, char *cmd_dest, size_t max_len)
{
    char cmdline_path[256];
    snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);

    FILE *f = fopen(cmdline_path, "r");
    if (f) {
        size_t n = fread(cmd_dest, 1, max_len - 1, f);
        fclose(f);
        if (n > 0) {
            cmd_dest[n] = '\0';
            /* In /proc/[pid]/cmdline, arguments are separated by '\0'; replace with spaces */
            for (size_t i = 0; i < n - 1; i++) {
                if (cmd_dest[i] == '\0')
                    cmd_dest[i] = ' ';
            }
            return;
        }
    }

    /* If cmdline is empty (kernel thread or permission denied), use [comm] */
    snprintf(cmd_dest, max_len, "[%s]", comm);
}
#endif

int builtin_ps(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    (void)state;

#ifndef _WIN32
    DIR *dir = opendir("/proc");
    if (!dir) {
        fprintf(stderr, "myshell: ps: cannot open /proc: %s\n", strerror(errno));
        return 1;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0)
        ticks_per_sec = 100;

    printf("%s%5s %5s %-12s %-4s %8s %s%s\n",
           COLOR_BOLD, "PID", "PPID", "USER", "STAT", "TIME", "COMMAND", COLOR_RESET);

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        /* Filter only numeric directories: /proc/[pid]/ */
        if (!isdigit(ent->d_name[0]))
            continue;

        int pid = atoi(ent->d_name);
        char stat_path[256];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

        FILE *f = fopen(stat_path, "r");
        if (!f) continue;

        char buf[1024];
        if (!fgets(buf, sizeof(buf), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        char comm[64] = "unknown";
        char state_char = '?';
        int ppid = 0;
        unsigned long utime = 0, stime = 0;

        char *open_p = strchr(buf, '(');
        char *close_p = strrchr(buf, ')');
        if (open_p && close_p && close_p > open_p) {
            int comm_len = close_p - open_p - 1;
            if (comm_len > 63) comm_len = 63;
            strncpy(comm, open_p + 1, comm_len);
            comm[comm_len] = '\0';

            /* Parse fields after ')' */
            sscanf(close_p + 2, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                   &state_char, &ppid, &utime, &stime);
        }

        /* Calculate CPU time: (utime + stime) / clock_ticks */
        unsigned long total_sec = (utime + stime) / ticks_per_sec;
        unsigned long mins = total_sec / 60;
        unsigned long secs = total_sec % 60;
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02lu:%02lu", mins, secs);

        char user[32];
        get_process_user(pid, user, sizeof(user));

        char cmd[256];
        get_process_cmdline(pid, comm, cmd, sizeof(cmd));

        const char *state_color = (state_char == 'R') ? COLOR_BRIGHT_GREEN : COLOR_RESET;

        printf("%5d %5d %-12s %s%-4c%s %8s %s\n",
               pid, ppid, user, state_color, state_char, COLOR_RESET, time_str, cmd);
    }

    closedir(dir);
    return 0;

#else
    printf("MyShell Native ps: Process table introspection requires Linux/WSL2 /proc environment.\n");
    return 0;
#endif
}
