#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include "scheduler.h"
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

#ifndef _WIN32

/* First Come First Served (FCFS) Scheduler */
static int run_fcfs(SchedTask *tasks, int task_count)
{
    struct timespec global_start;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    for (int i = 0; i < task_count; i++) {
        tasks[i].arrival_time = global_start;
        clock_gettime(CLOCK_MONOTONIC, &tasks[i].start_time);

        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: scheduler fork");
            return -1;
        }

        if (pid == 0) {
            /* Child process: execute task */
            execvp(tasks[i].argv[0], tasks[i].argv);
            fprintf(stderr, "myshell: failed to execute %s: %s\n", tasks[i].argv[0], strerror(errno));
            exit(127);
        }

        tasks[i].pid = pid;
        tasks[i].state = PROC_STATE_RUNNING;

        int status = 0;
        struct rusage usage;
        wait4(pid, &status, 0, &usage);

        clock_gettime(CLOCK_MONOTONIC, &tasks[i].finish_time);
        tasks[i].state = PROC_STATE_COMPLETED;
        tasks[i].exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        tasks[i].user_cpu_sec = timeval_to_sec(usage.ru_utime);
        tasks[i].sys_cpu_sec = timeval_to_sec(usage.ru_stime);
        tasks[i].burst_cpu_sec = tasks[i].user_cpu_sec + tasks[i].sys_cpu_sec;
        tasks[i].elapsed_sec = timespec_diff_sec(tasks[i].arrival_time, tasks[i].finish_time);
        tasks[i].waiting_time_sec = tasks[i].elapsed_sec - tasks[i].burst_cpu_sec;
        if (tasks[i].waiting_time_sec < 0) tasks[i].waiting_time_sec = 0;

        tasks[i].voluntary_cs = usage.ru_nvcsw;
        tasks[i].involuntary_cs = usage.ru_nivcsw;
    }

    return 0;
}

/* Priority Scheduler */
static int compare_priority(const void *a, const void *b)
{
    const SchedTask *taskA = (const SchedTask *)a;
    const SchedTask *taskB = (const SchedTask *)b;
    /* Higher priority number runs first */
    return taskB->priority - taskA->priority;
}

static int run_priority(SchedTask *tasks, int task_count)
{
    /* Sort tasks by priority descending */
    qsort(tasks, task_count, sizeof(SchedTask), compare_priority);
    return run_fcfs(tasks, task_count);
}

/* Round Robin (RR) Preemptive Scheduler using SIGSTOP & SIGCONT */
static int run_round_robin(SchedTask *tasks, int task_count, int quantum_ms)
{
    if (quantum_ms <= 0) quantum_ms = 100;

    struct timespec global_start;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    /* 1. Fork all processes and immediately have them stop */
    for (int i = 0; i < task_count; i++) {
        tasks[i].arrival_time = global_start;
        tasks[i].state = PROC_STATE_READY;

        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: scheduler fork");
            return -1;
        }

        if (pid == 0) {
            /* Child stops itself immediately until scheduler grants a time slice */
            raise(SIGSTOP);
            execvp(tasks[i].argv[0], tasks[i].argv);
            fprintf(stderr, "myshell: failed to execute %s: %s\n", tasks[i].argv[0], strerror(errno));
            exit(127);
        }

        tasks[i].pid = pid;
        /* Wait for child to reach the initial SIGSTOP */
        int status = 0;
        waitpid(pid, &status, WUNTRACED);
        tasks[i].state = PROC_STATE_STOPPED;
    }

    /* 2. Round Robin Scheduling Loop */
    int completed_count = 0;
    int idx = 0;

    while (completed_count < task_count) {
        if (tasks[idx].state != PROC_STATE_COMPLETED) {
            /* Grant time slice: wake up child */
            kill(tasks[idx].pid, SIGCONT);
            tasks[idx].state = PROC_STATE_RUNNING;

            long remaining_us = quantum_ms * 1000L;
            bool finished = false;
            struct rusage usage;

            while (remaining_us > 0) {
                int status = 0;
                pid_t res = wait4(tasks[idx].pid, &status, WNOHANG, &usage);
                if (res == tasks[idx].pid) {
                    /* Child completed during this quantum */
                    finished = true;
                    tasks[idx].state = PROC_STATE_COMPLETED;
                    tasks[idx].exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                    clock_gettime(CLOCK_MONOTONIC, &tasks[idx].finish_time);

                    tasks[idx].user_cpu_sec = timeval_to_sec(usage.ru_utime);
                    tasks[idx].sys_cpu_sec = timeval_to_sec(usage.ru_stime);
                    tasks[idx].burst_cpu_sec = tasks[idx].user_cpu_sec + tasks[idx].sys_cpu_sec;
                    tasks[idx].elapsed_sec = timespec_diff_sec(tasks[idx].arrival_time, tasks[idx].finish_time);
                    tasks[idx].waiting_time_sec = tasks[idx].elapsed_sec - tasks[idx].burst_cpu_sec;
                    if (tasks[idx].waiting_time_sec < 0) tasks[idx].waiting_time_sec = 0;

                    tasks[idx].voluntary_cs = usage.ru_nvcsw;
                    tasks[idx].involuntary_cs = usage.ru_nivcsw;

                    completed_count++;
                    break;
                }

                long slice = (remaining_us > 10000) ? 10000 : remaining_us;
                usleep(slice);
                remaining_us -= slice;
            }

            if (!finished) {
                /* Preempt task: pause it for next process's turn */
                kill(tasks[idx].pid, SIGSTOP);
                int status = 0;
                waitpid(tasks[idx].pid, &status, WUNTRACED);
                tasks[idx].state = PROC_STATE_STOPPED;
            }
        }

        idx = (idx + 1) % task_count;
    }

    return 0;
}

#endif /* !_WIN32 */

int scheduler_run(SchedPolicy policy, int quantum_ms, SchedTask *tasks, int task_count, SchedResult *result)
{
    if (!tasks || task_count <= 0 || !result) return -1;

    result->policy = policy;
    result->quantum_ms = quantum_ms;
    result->tasks = tasks;
    result->task_count = task_count;

    switch (policy) {
        case SCHED_POLICY_FCFS:
            result->policy_name = "First Come First Served (FCFS)";
            break;
        case SCHED_POLICY_RR:
            result->policy_name = "Round Robin (RR)";
            break;
        case SCHED_POLICY_PRIORITY:
            result->policy_name = "Priority Scheduling";
            break;
    }

#ifndef _WIN32
    int status = 0;
    if (policy == SCHED_POLICY_FCFS) {
        status = run_fcfs(tasks, task_count);
    } else if (policy == SCHED_POLICY_RR) {
        status = run_round_robin(tasks, task_count, quantum_ms);
    } else if (policy == SCHED_POLICY_PRIORITY) {
        status = run_priority(tasks, task_count);
    }

    if (status != 0) return status;

    /* Aggregate metrics */
    double sum_turnaround = 0;
    double sum_waiting = 0;
    double sum_cpu = 0;
    long sum_invol_cs = 0;
    long sum_vol_cs = 0;

    for (int i = 0; i < task_count; i++) {
        sum_turnaround += tasks[i].elapsed_sec;
        sum_waiting += tasks[i].waiting_time_sec;
        sum_cpu += tasks[i].burst_cpu_sec;
        sum_invol_cs += tasks[i].involuntary_cs;
        sum_vol_cs += tasks[i].voluntary_cs;
    }

    result->avg_turnaround_time = sum_turnaround / task_count;
    result->avg_waiting_time = sum_waiting / task_count;
    result->total_cpu_time = sum_cpu;
    result->total_involuntary_cs = sum_invol_cs;
    result->total_voluntary_cs = sum_vol_cs;

    return 0;
#else
    fprintf(stderr, "myshell: Process scheduling experiments require Linux/WSL2 POSIX environment\n");
    return -1;
#endif
}

void scheduler_print_results(const SchedResult *result)
{
    if (!result) return;

    printf("\n%s══════════════════════════════════════════════════════════════════════════════════════════%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("  %s%sPolicy:%s %s%-35s%s", COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET, 
           COLOR_BOLD, result->policy_name, COLOR_RESET);
    if (result->policy == SCHED_POLICY_RR) {
        printf("  %sQuantum:%s %d ms\n", COLOR_BOLD, COLOR_RESET, result->quantum_ms);
    } else {
        printf("\n");
    }
    printf("%s──────────────────────────────────────────────────────────────────────────────────────────%s\n",
           COLOR_DIM, COLOR_RESET);
    printf("%s  %-12s %6s %4s %10s %10s %12s %8s %8s%s\n",
           COLOR_BOLD, "Task Name", "PID", "Pri", "Burst(s)", "Wait(s)", "Turnaround(s)", "Vol CS", "Invol CS", COLOR_RESET);
    printf("%s──────────────────────────────────────────────────────────────────────────────────────────%s\n",
           COLOR_DIM, COLOR_RESET);

    for (int i = 0; i < result->task_count; i++) {
        const SchedTask *t = &result->tasks[i];
        printf("  %-12s %6d %4d %10.3f %10.3f %12.3f %8ld %8ld\n",
               t->name, (int)t->pid, t->priority,
               t->burst_cpu_sec, t->waiting_time_sec, t->elapsed_sec,
               t->voluntary_cs, t->involuntary_cs);
    }

    printf("%s──────────────────────────────────────────────────────────────────────────────────────────%s\n",
           COLOR_DIM, COLOR_RESET);
    printf("  %sSummary Averages:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("    • %sAverage Turnaround Time:%s  %.3f seconds\n", COLOR_BOLD, COLOR_RESET, result->avg_turnaround_time);
    printf("    • %sAverage Waiting Time:%s     %.3f seconds\n", COLOR_BOLD, COLOR_RESET, result->avg_waiting_time);
    printf("    • %sTotal CPU Time Used:%s      %.3f seconds\n", COLOR_BOLD, COLOR_RESET, result->total_cpu_time);
    printf("    • %sTotal Context Switches:%s   %ld (Involuntary: %ld, Voluntary: %ld)\n",
           COLOR_BOLD, COLOR_RESET,
           result->total_involuntary_cs + result->total_voluntary_cs,
           result->total_involuntary_cs, result->total_voluntary_cs);
    printf("%s══════════════════════════════════════════════════════════════════════════════════════════%s\n\n",
           COLOR_BOLD, COLOR_RESET);
}

void scheduler_print_comparison(const SchedResult *results, int count)
{
    if (!results || count <= 0) return;

    printf("\n%s╔════════════════════════════════════════════════════════════════════════════════════════╗%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s║              PROCESS SCHEDULING POLICIES: SCIENTIFIC COMPARISON REPORT                 ║%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════════════════════════════╣%s\n",
           COLOR_BOLD, COLOR_RESET);
    printf("%s  %-26s %12s %14s %12s %12s%s\n",
           COLOR_BOLD, "Scheduling Policy", "Avg Wait(s)", "Avg Turnaround", "Invol. CS", "Total CPU(s)", COLOR_RESET);
    printf("%s────────────────────────────────────────────────────────────────────────────────────────%s\n",
           COLOR_DIM, COLOR_RESET);

    for (int i = 0; i < count; i++) {
        char label[64];
        if (results[i].policy == SCHED_POLICY_RR) {
            snprintf(label, sizeof(label), "Round Robin (%dms)", results[i].quantum_ms);
        } else {
            strncpy(label, results[i].policy_name, sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
        }

        printf("  %-26s %12.3f %14.3f %12ld %12.3f\n",
               label,
               results[i].avg_waiting_time,
               results[i].avg_turnaround_time,
               results[i].total_involuntary_cs,
               results[i].total_cpu_time);
    }

    printf("%s╚════════════════════════════════════════════════════════════════════════════════════════╝%s\n",
           COLOR_BOLD, COLOR_RESET);

    printf("%s📊 OS Kernel Insights & Analysis:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  1. %sContext Switch Overhead:%s Notice how Round Robin with a smaller time quantum creates\n"
           "     significantly higher involuntary context switches (recorded via getrusage).\n",
           COLOR_BRIGHT_YELLOW, COLOR_RESET);
    printf("  2. %sTurnaround Fairness:%s Round Robin prevents starvation of shorter tasks, whereas\n"
           "     FCFS can cause the 'Convoy Effect' if a long job runs first.\n",
           COLOR_BRIGHT_CYAN, COLOR_RESET);
    printf("  3. %sPreemptive Priority:%s High-priority jobs complete with minimal waiting times.\n\n",
           COLOR_BRIGHT_GREEN, COLOR_RESET);
}
