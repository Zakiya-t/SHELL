#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

typedef struct {
    int pid;
    char user[32];
    char state;
    long res_kb;
    char comm[64];
} ProcessInfo;

#ifndef _WIN32
static struct termios orig_termios;
static bool raw_mode_active = false;

static void disable_raw_mode(void)
{
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = false;
        /* Exit alternate screen and show cursor */
        printf("\033[?1049l\033[?25h");
        fflush(stdout);
    }
}

static void enable_raw_mode(void)
{
    if (!isatty(STDIN_FILENO))
        return;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = true;
    /* Switch to alternate screen and hide cursor */
    printf("\033[?1049h\033[?25l");
    fflush(stdout);
}

static bool check_quit_key(void)
{
    if (!isatty(STDIN_FILENO))
        return false;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv = {1, 0}; /* Wait up to 1 second */
    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    if (ret > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q' || c == 3) /* 'q' or Ctrl+C */
                return true;
        }
    }
    return false;
}

static void read_mem_stats(long *total_kb, long *free_kb, long *avail_kb)
{
    *total_kb = *free_kb = *avail_kb = 0;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", total_kb);
        else if (strncmp(line, "MemFree:", 8) == 0)
            sscanf(line + 8, "%ld", free_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", avail_kb);
    }
    fclose(f);
}

static void read_cpu_ticks(unsigned long long *work, unsigned long long *total)
{
    *work = *total = 0;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    char line[256];
    if (fgets(line, sizeof(line), f) && strncmp(line, "cpu ", 4) == 0) {
        unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
        if (sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
            *work = user + nice + sys + irq + softirq + steal;
            *total = *work + idle + iowait;
        }
    }
    fclose(f);
}

static int collect_processes(ProcessInfo *procs, int max_procs, int *running_count, int *sleeping_count)
{
    *running_count = 0;
    *sleeping_count = 0;
    int count = 0;

    DIR *dir = opendir("/proc");
    if (!dir) return 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!isdigit(ent->d_name[0]))
            continue;

        int pid = atoi(ent->d_name);
        char path[256];

        /* Read process comm */
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            char comm[64] = "unknown";
            char state_char = '?';
            char *open_p = strchr(buf, '(');
            char *close_p = strrchr(buf, ')');

            if (open_p && close_p && close_p > open_p) {
                int comm_len = close_p - open_p - 1;
                if (comm_len > 63) comm_len = 63;
                strncpy(comm, open_p + 1, comm_len);
                comm[comm_len] = '\0';
                state_char = *(close_p + 2);
            }

            if (state_char == 'R')
                (*running_count)++;
            else
                (*sleeping_count)++;

            if (count < max_procs) {
                procs[count].pid = pid;
                strncpy(procs[count].comm, comm, sizeof(procs[count].comm) - 1);
                procs[count].state = state_char;
                procs[count].res_kb = 0;

                /* Read memory (VmRSS) from /proc/[pid]/status */
                char status_path[256];
                snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
                FILE *fs = fopen(status_path, "r");
                if (fs) {
                    char sline[256];
                    while (fgets(sline, sizeof(sline), fs)) {
                        if (strncmp(sline, "VmRSS:", 6) == 0) {
                            sscanf(sline + 6, "%ld", &procs[count].res_kb);
                            break;
                        }
                    }
                    fclose(fs);
                }
                count++;
            }
        }
        fclose(f);
    }
    closedir(dir);
    return count;
}
#endif

int builtin_top(int argc, char **argv, ShellState *state)
{
    (void)state;

#ifndef _WIN32
    int max_iterations = -1; /* default: run until 'q' */
    if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
        max_iterations = atoi(argv[2]);
    }

    if (!isatty(STDIN_FILENO) && max_iterations == -1) {
        max_iterations = 1;
    }

    enable_raw_mode();

    unsigned long long prev_work = 0, prev_total = 0;
    read_cpu_ticks(&prev_work, &prev_total);

    int iter = 0;
    while (max_iterations == -1 || iter < max_iterations) {
        iter++;

        /* Clear screen & move to top */
        printf("\033[H\033[2J");

        /* Current time */
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_now);

        /* Read live kernel stats */
        unsigned long long work = 0, total = 0;
        read_cpu_ticks(&work, &total);
        double cpu_pct = 0.0;
        if (total > prev_total) {
            cpu_pct = ((double)(work - prev_work) / (double)(total - prev_total)) * 100.0;
        }
        prev_work = work;
        prev_total = total;

        long mem_total = 0, mem_free = 0, mem_avail = 0;
        read_mem_stats(&mem_total, &mem_free, &mem_avail);
        long mem_used = mem_total - mem_avail;

        ProcessInfo procs[32];
        int running_count = 0, sleeping_count = 0;
        int num_procs = collect_processes(procs, 18, &running_count, &sleeping_count);

        /* Header row 1: top banner */
        printf("%stop - %s up,  %d tasks: %d running, %d sleeping%s\n",
               COLOR_BOLD, time_str, num_procs, running_count, sleeping_count, COLOR_RESET);

        /* Header row 2: CPU stats from kernel */
        printf("%%Cpu(s): %s%5.1f%%%s user/sys, %s%5.1f%%%s idle  [Linux Kernel /proc/stat]\n",
               COLOR_BRIGHT_GREEN, cpu_pct, COLOR_RESET,
               COLOR_DIM, 100.0 - cpu_pct, COLOR_RESET);

        /* Header row 3: Memory stats from kernel */
        printf("MiB Mem : %s%6.1f%s total, %s%6.1f%s free, %s%6.1f%s used  [/proc/meminfo]\n",
               COLOR_BOLD, (double)mem_total / 1024.0, COLOR_RESET,
               COLOR_BRIGHT_GREEN, (double)mem_free / 1024.0, COLOR_RESET,
               COLOR_BRIGHT_YELLOW, (double)mem_used / 1024.0, COLOR_RESET);

        printf("\n%s\033[7m  PID   S      RES  COMMAND                                   \033[0m\n", COLOR_BOLD);

        for (int i = 0; i < num_procs && i < 15; i++) {
            char size_str[16];
            if (procs[i].res_kb < 1024)
                snprintf(size_str, sizeof(size_str), "%4ld KB", procs[i].res_kb);
            else
                snprintf(size_str, sizeof(size_str), "%4.1f MB", (double)procs[i].res_kb / 1024.0);

            const char *state_color = (procs[i].state == 'R') ? COLOR_BRIGHT_GREEN : COLOR_DIM;

            printf("%5d   %s%c%s   %8s  %-35s\n",
                   procs[i].pid,
                   state_color, procs[i].state, COLOR_RESET,
                   size_str,
                   procs[i].comm);
        }

        printf("\n%s[Direct Linux Kernel /proc reader]  Press %s'q'%s to exit%s\n",
               COLOR_DIM, COLOR_BOLD, COLOR_RESET, COLOR_RESET);
        fflush(stdout);

        if (max_iterations != -1 && iter >= max_iterations)
            break;

        if (check_quit_key())
            break;
    }

    disable_raw_mode();
    putchar('\n');
    return 0;

#else
    (void)argc;
    (void)argv;
    printf("MyShell Native Top: Running in Win32 environment.\n");
    printf("On Linux/WSL2, this directly samples /proc/stat, /proc/meminfo, and /proc/[pid]/.\n");
    return 0;
#endif
}
