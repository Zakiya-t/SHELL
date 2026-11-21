#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtins.h"
#include "scheduler.h"
#include "ui.h"

/*
 * Built-in: sched-run
 * Usage:
 *   sched-run fcfs <cmd...>
 *   sched-run rr <quantum_ms> <cmd...>
 *   sched-run priority <pri1:cmd1> <pri2:cmd2> ...
 */
int builtin_sched_run(int argc, char **argv, ShellState *state)
{
    (void)state;
    if (argc < 3) {
        printf("%sUsage:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("  sched-run fcfs <cmd1> <cmd2> ...\n");
        printf("  sched-run rr <quantum_ms> <cmd1> <cmd2> ...\n");
        printf("  sched-run priority <pri1:cmd1> <pri2:cmd2> ... (e.g. 10:./taskA 2:./taskB)\n");
        return 1;
    }

    SchedPolicy policy;
    int quantum_ms = 100;
    int task_start_idx = 2;

    if (strcmp(argv[1], "fcfs") == 0) {
        policy = SCHED_POLICY_FCFS;
        task_start_idx = 2;
    } else if (strcmp(argv[1], "rr") == 0) {
        policy = SCHED_POLICY_RR;
        quantum_ms = atoi(argv[2]);
        if (quantum_ms <= 0) quantum_ms = 100;
        task_start_idx = 3;
    } else if (strcmp(argv[1], "priority") == 0) {
        policy = SCHED_POLICY_PRIORITY;
        task_start_idx = 2;
    } else {
        fprintf(stderr, "myshell: unknown policy '%s'. Use fcfs, rr, or priority.\n", argv[1]);
        return 1;
    }

    int task_count = argc - task_start_idx;
    if (task_count <= 0) {
        fprintf(stderr, "myshell: no tasks provided to schedule.\n");
        return 1;
    }

    SchedTask *tasks = calloc(task_count, sizeof(SchedTask));
    if (!tasks) {
        perror("myshell: malloc");
        return 1;
    }

    for (int i = 0; i < task_count; i++) {
        char *arg_copy = strdup(argv[task_start_idx + i]);
        tasks[i].id = i + 1;
        tasks[i].priority = 5; /* Default priority */

        if (policy == SCHED_POLICY_PRIORITY) {
            char *colon = strchr(arg_copy, ':');
            if (colon) {
                *colon = '\0';
                tasks[i].priority = atoi(arg_copy);
                memmove(arg_copy, colon + 1, strlen(colon + 1) + 1);
            }
        }

        /* Split command and args */
        tasks[i].argv = malloc(sizeof(char *) * 16);
        tasks[i].argc = 0;
        char *token = strtok(arg_copy, " ");
        while (token && tasks[i].argc < 15) {
            tasks[i].argv[tasks[i].argc++] = strdup(token);
            token = strtok(NULL, " ");
        }
        tasks[i].argv[tasks[i].argc] = NULL;
        snprintf(tasks[i].name, sizeof(tasks[i].name), "Task-%d", i + 1);
        free(arg_copy);
    }

    printf("%s[Scheduler] Starting execution of %d task(s)...%s\n", COLOR_CYAN, task_count, COLOR_RESET);

    SchedResult result;
    int ret = scheduler_run(policy, quantum_ms, tasks, task_count, &result);
    if (ret == 0) {
        scheduler_print_results(&result);
    }

    /* Cleanup */
    for (int i = 0; i < task_count; i++) {
        for (int j = 0; j < tasks[i].argc; j++) {
            free(tasks[i].argv[j]);
        }
        free(tasks[i].argv);
    }
    free(tasks);

    return ret;
}

/*
 * Built-in: sched-bench
 * Automated comparative benchmark suite.
 * Runs 3 standard CPU workloads across FCFS, RR(50ms), RR(200ms), and Priority,
 * and prints an empirical comparison matrix.
 */
int builtin_sched_bench(int argc, char **argv, ShellState *state)
{
    (void)state;
    unsigned long base_work = 25000000UL;
    if (argc > 1) {
        unsigned long user_work = strtoul(argv[1], NULL, 10);
        if (user_work > 0) base_work = user_work;
    }

    /* Check if workload binary exists, if not, alert user */
    const char *workload_bin = "./workload";
    if (access(workload_bin, X_OK) != 0) {
        workload_bin = "build/workload";
        if (access(workload_bin, X_OK) != 0) {
            fprintf(stderr, "myshell: benchmark workload binary not found. Please compile benchmarks/workload.c\n");
            return 1;
        }
    }

    printf("%s══════════════════════════════════════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("  %s%sMYSSHELL SCIENTIFIC PROCESS SCHEDULER & PERFORMANCE BENCHMARK SUITE%s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
    printf("  Executing 3 real CPU-intensive workloads under multiple scheduling algorithms...\n");
    printf("%s══════════════════════════════════════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_RESET);

    char iter_short[32], iter_med[32], iter_long[32];
    snprintf(iter_short, sizeof(iter_short), "%lu", base_work / 2);
    snprintf(iter_med, sizeof(iter_med), "%lu", base_work);
    snprintf(iter_long, sizeof(iter_long), "%lu", base_work * 2);

    SchedResult results[4];
    SchedPolicy policies[4] = { SCHED_POLICY_FCFS, SCHED_POLICY_RR, SCHED_POLICY_RR, SCHED_POLICY_PRIORITY };
    int quantums[4] = { 0, 50, 200, 0 };
    int num_tests = 4;

    for (int t = 0; t < num_tests; t++) {
        SchedTask *tasks = calloc(3, sizeof(SchedTask));

        /* Task 1: Short */
        tasks[0].id = 1;
        tasks[0].priority = 10; /* Highest priority */
        snprintf(tasks[0].name, sizeof(tasks[0].name), "Short-Job");
        tasks[0].argv = malloc(sizeof(char *) * 4);
        tasks[0].argv[0] = strdup(workload_bin);
        tasks[0].argv[1] = strdup("Short");
        tasks[0].argv[2] = strdup(iter_short);
        tasks[0].argv[3] = NULL;
        tasks[0].argc = 3;

        /* Task 2: Medium */
        tasks[1].id = 2;
        tasks[1].priority = 5;
        snprintf(tasks[1].name, sizeof(tasks[1].name), "Medium-Job");
        tasks[1].argv = malloc(sizeof(char *) * 4);
        tasks[1].argv[0] = strdup(workload_bin);
        tasks[1].argv[1] = strdup("Medium");
        tasks[1].argv[2] = strdup(iter_med);
        tasks[1].argv[3] = NULL;
        tasks[1].argc = 3;

        /* Task 3: Long */
        tasks[2].id = 3;
        tasks[2].priority = 1; /* Lowest priority */
        snprintf(tasks[2].name, sizeof(tasks[2].name), "Long-Job");
        tasks[2].argv = malloc(sizeof(char *) * 4);
        tasks[2].argv[0] = strdup(workload_bin);
        tasks[2].argv[1] = strdup("Long");
        tasks[2].argv[2] = strdup(iter_long);
        tasks[2].argv[3] = NULL;
        tasks[2].argc = 3;

        printf("  %s▶ Running Test %d/4: %s...%s\n", 
               COLOR_YELLOW, t + 1,
               (t == 0) ? "FCFS" : (t == 1) ? "Round Robin (50ms)" : (t == 2) ? "Round Robin (200ms)" : "Priority",
               COLOR_RESET);
        fflush(stdout);

        scheduler_run(policies[t], quantums[t], tasks, 3, &results[t]);

        /* Don't free tasks yet, results[t].tasks points to it */
    }

    /* Print master side-by-side comparison */
    scheduler_print_comparison(results, num_tests);

    /* Free all allocated tasks */
    for (int t = 0; t < num_tests; t++) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < results[t].tasks[i].argc; j++) {
                free(results[t].tasks[i].argv[j]);
            }
            free(results[t].tasks[i].argv);
        }
        free(results[t].tasks);
    }

    return 0;
}
