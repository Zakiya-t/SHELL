#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include "builtins.h"
#include "ui.h"

static double timeval_to_sec(struct timeval tv)
{
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static double timespec_diff_sec(struct timespec start, struct timespec end)
{
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return sec + nsec / 1000000000.0;
}

/*
 * Built-in: inspect
 * Runs any command and exposes Linux Kernel Subsystem Decisions:
 * - Process Subsystem (PID, exit code, signal)
 * - Scheduler Subsystem (CFS, CPU time, voluntary/involuntary context switches)
 * - Memory Subsystem (Max RSS, minor/major page faults)
 * - VFS Subsystem (I/O operations)
 *
 * Usage: inspect <command> [args...]
 */
int builtin_inspect(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 2) {
        printf("%sUsage:%s inspect <command> [args...]\n", COLOR_BOLD, COLOR_RESET);
        printf("Example: inspect ls -la\n");
        printf("         inspect gcc main.c\n");
        return 1;
    }

#ifndef _WIN32
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    pid_t pid = fork();
    if (pid < 0) {
        perror("myshell: inspect fork");
        return 1;
    }

    if (pid == 0) {
        /* Child executes target command */
        execvp(argv[1], &argv[1]);
        if (errno == ENOENT) {
            fprintf(stderr, "myshell: %s: command not found\n", argv[1]);
            exit(127);
        } else {
            fprintf(stderr, "myshell: %s: %s\n", argv[1], strerror(errno));
            exit(126);
        }
    }

    /* Parent waits and collects kernel rusage data */
    int status = 0;
    struct rusage usage;
    wait4(pid, &status, 0, &usage);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double wall_time = timespec_diff_sec(start_time, end_time);
    double user_cpu = timeval_to_sec(usage.ru_utime);
    double sys_cpu = timeval_to_sec(usage.ru_stime);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    /* Render the Kernel Decision & Subsystem Report */
    printf("\n%s══════════════════════════════════════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("  %s%s🔍 LINUX KERNEL SUBSYSTEM DECISION & OBSERVABILITY REPORT%s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
    printf("  Target Command: %s%-45s%s [PID: %d]\n", COLOR_BOLD, argv[1], COLOR_RESET, (int)pid);
    printf("%s──────────────────────────────────────────────────────────────────────────────────────────%s\n",
           COLOR_DIM, COLOR_RESET);

    /* 1. Process Subsystem */
    printf("  %s1. Process Lifecycle Subsystem:%s\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("     • Assigned PID:             %d (allocated by kernel process table)\n", (int)pid);
    if (WIFEXITED(status)) {
        printf("     • Process Termination:      Clean Exit (Exit Code: %s%d%s)\n",
               (exit_code == 0) ? COLOR_GREEN : COLOR_RED, exit_code, COLOR_RESET);
    } else if (WIFSIGNALED(status)) {
        printf("     • Process Termination:      Killed by Signal %d (%s)\n",
               WTERMSIG(status), strsignal(WTERMSIG(status)));
    }

    /* 2. CPU Scheduler Subsystem */
    printf("  %s2. CPU Scheduler Subsystem (CFS):%s\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("     • Kernel Scheduling Class:  SCHED_OTHER (Completely Fair Scheduler)\n");
    printf("     • Wall-Clock Elapsed Time:  %.4f seconds\n", wall_time);
    printf("     • User-Space CPU Time:      %.4f seconds (running user instructions)\n", user_cpu);
    printf("     • Kernel-Space CPU Time:    %.4f seconds (executing system calls)\n", sys_cpu);
    printf("     • Voluntary Context Switches:   %ld (process yielded for I/O / sleep)\n", usage.ru_nvcsw);
    printf("     • Involuntary Context Switches: %ld (kernel CFS preempted process)\n", usage.ru_nivcsw);

    /* 3. Memory Management Subsystem */
    printf("  %s3. Memory Management Subsystem:%s\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("     • Peak Resident Set (RSS):  %ld KB (physical RAM allocated)\n", usage.ru_maxrss);
    printf("     • Minor Page Faults:        %ld (mapped in RAM without disk access)\n", usage.ru_minflt);
    printf("     • Major Page Faults:        %ld (read from storage swap/disk)\n", usage.ru_majflt);

    /* 4. Virtual File System (VFS) Subsystem */
    printf("  %s4. Virtual File System (VFS) & I/O Subsystem:%s\n", COLOR_BRIGHT_GREEN, COLOR_RESET);
    printf("     • Block Input Operations:   %ld operations\n", usage.ru_inblock);
    printf("     • Block Output Operations:  %ld operations\n", usage.ru_oublock);
    printf("%s══════════════════════════════════════════════════════════════════════════════════════════%s\n\n",
           COLOR_BOLD, COLOR_RESET);

    return exit_code;
#else
    fprintf(stderr, "myshell: inspect requires Linux/WSL2 POSIX environment\n");
    return 1;
#endif
}

/*
 * Built-in: telemetry
 * Toggles live kernel telemetry badge after every command.
 * Usage: telemetry [on|off]
 */
int builtin_telemetry(int argc, char **argv, ShellState *state)
{
    if (argc < 2) {
        printf("Telemetry mode is currently: %s%s%s\n",
               COLOR_BOLD, state->telemetry_enabled ? "ON" : "OFF", COLOR_RESET);
        printf("Usage: telemetry on | telemetry off\n");
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        state->telemetry_enabled = true;
        printf("%s[✔] Kernel Subsystem Telemetry Badge: ACTIVATED%s\n", COLOR_GREEN, COLOR_RESET);
    } else if (strcmp(argv[1], "off") == 0) {
        state->telemetry_enabled = false;
        printf("%s[✔] Kernel Subsystem Telemetry Badge: DEACTIVATED%s\n", COLOR_YELLOW, COLOR_RESET);
    } else {
        fprintf(stderr, "Usage: telemetry on | telemetry off\n");
        return 1;
    }

    return 0;
}
