#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#ifndef _WIN32
#include <sys/resource.h>
#endif

typedef enum {
    SCHED_POLICY_FCFS,
    SCHED_POLICY_RR,
    SCHED_POLICY_PRIORITY
} SchedPolicy;

typedef enum {
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_STOPPED,
    PROC_STATE_COMPLETED
} ProcessState;

typedef struct {
    int id;
    pid_t pid;
    char name[64];
    char **argv;
    int argc;
    int priority;                  /* 1 (Lowest) to 10 (Highest) */
    ProcessState state;
    
    struct timespec arrival_time;
    struct timespec start_time;
    struct timespec finish_time;
    
    double burst_cpu_sec;          /* Total CPU time consumed (User + Sys) */
    double user_cpu_sec;           /* User-space CPU time */
    double sys_cpu_sec;            /* Kernel-space CPU time */
    double elapsed_sec;            /* Wall-clock turnaround time */
    double waiting_time_sec;       /* Waiting time = Turnaround - Burst */
    
    long voluntary_cs;             /* Voluntary context switches */
    long involuntary_cs;           /* Involuntary context switches */
    int exit_status;
} SchedTask;

typedef struct {
    SchedPolicy policy;
    const char *policy_name;
    int quantum_ms;                /* Time slice for Round Robin in milliseconds */
    SchedTask *tasks;
    int task_count;
    
    double avg_turnaround_time;
    double avg_waiting_time;
    double total_cpu_time;
    long total_involuntary_cs;
    long total_voluntary_cs;
} SchedResult;

/* Scheduler execution engine */
int scheduler_run(SchedPolicy policy, int quantum_ms, SchedTask *tasks, int task_count, SchedResult *result);

/* Performance report visualizer */
void scheduler_print_results(const SchedResult *result);

/* Side-by-side comparative benchmark analyzer */
void scheduler_print_comparison(const SchedResult *results, int count);

#endif /* SCHEDULER_H */
