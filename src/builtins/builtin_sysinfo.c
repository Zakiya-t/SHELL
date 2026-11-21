#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/utsname.h>
#endif

static void get_os_name(char *dest, size_t max_len)
{
#ifndef _WIN32
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *val = line + 12;
                if (*val == '"') val++;
                char *end = strchr(val, '"');
                if (!end) end = strchr(val, '\n');
                if (end) *end = '\0';
                strncpy(dest, val, max_len - 1);
                dest[max_len - 1] = '\0';
                fclose(f);
                return;
            }
        }
        fclose(f);
    }

    struct utsname un;
    if (uname(&un) == 0) {
        snprintf(dest, max_len, "%s %s", un.sysname, un.release);
    } else {
        strncpy(dest, "Linux", max_len);
    }
#else
    (void)dest;
    (void)max_len;
    strncpy(dest, "Windows (WSL / Win32 Environment)", max_len);
#endif
}

static void get_cpu_info(char *dest, size_t max_len, int *cores)
{
    *cores = 0;
    strncpy(dest, "Unknown CPU", max_len);

#ifndef _WIN32
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;

    char line[256];
    bool model_found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            (*cores)++;
        } else if (!model_found && strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                char *end = strchr(colon, '\n');
                if (end) *end = '\0';
                strncpy(dest, colon, max_len - 1);
                dest[max_len - 1] = '\0';
                model_found = true;
            }
        }
    }
    fclose(f);
#else
    const char *id = getenv("PROCESSOR_IDENTIFIER");
    const char *num = getenv("NUMBER_OF_PROCESSORS");
    if (id) strncpy(dest, id, max_len - 1);
    if (num) *cores = atoi(num);
#endif
}

static void get_mem_info(long *used_mb, long *total_mb, double *pct)
{
    *used_mb = *total_mb = 0;
    *pct = 0.0;

#ifndef _WIN32
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;

    long total_kb = 0, avail_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", &total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", &avail_kb);
    }
    fclose(f);

    if (total_kb > 0) {
        *total_mb = total_kb / 1024;
        *used_mb = (total_kb - avail_kb) / 1024;
        *pct = ((double)(*used_mb) / (double)(*total_mb)) * 100.0;
    }
#endif
}

static void get_uptime(char *dest, size_t max_len)
{
    strncpy(dest, "N/A", max_len);

#ifndef _WIN32
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return;

    double up_sec = 0;
    if (fscanf(f, "%lf", &up_sec) == 1) {
        long sec = (long)up_sec;
        long days = sec / 86400;
        long hours = (sec % 86400) / 3600;
        long mins = (sec % 3600) / 60;

        if (days > 0) {
            snprintf(dest, max_len, "%ld days, %ld hours, %ld mins", days, hours, mins);
        } else if (hours > 0) {
            snprintf(dest, max_len, "%ld hours, %ld mins", hours, mins);
        } else {
            snprintf(dest, max_len, "%ld mins", mins);
        }
    }
    fclose(f);
#endif
}

int builtin_sysinfo(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;

    const char *user = getenv("USER");
#ifdef _WIN32
    if (!user) user = getenv("USERNAME");
#endif
    if (!user) user = "user";

    char host[128] = "localhost";
#ifndef _WIN32
    if (gethostname(host, sizeof(host)) != 0 || strlen(host) == 0) {
        struct utsname un_h;
        if (uname(&un_h) == 0) {
            strncpy(host, un_h.nodename, sizeof(host) - 1);
        }
    }
#else
    const char *h = getenv("COMPUTERNAME");
    if (!h) h = getenv("HOSTNAME");
    if (h) strncpy(host, h, sizeof(host) - 1);
#endif

    char os_name[256];
    get_os_name(os_name, sizeof(os_name));

    char kernel_rel[128] = "N/A";
    char arch_str[64] = "x86_64";
#ifndef _WIN32
    struct utsname un;
    if (uname(&un) == 0) {
        strncpy(kernel_rel, un.release, sizeof(kernel_rel) - 1);
        strncpy(arch_str, un.machine, sizeof(arch_str) - 1);
    }
#else
    const char *a = getenv("PROCESSOR_ARCHITECTURE");
    if (a) strncpy(arch_str, a, sizeof(arch_str) - 1);
#endif

    char cpu_model[256];
    int cpu_cores = 0;
    get_cpu_info(cpu_model, sizeof(cpu_model), &cpu_cores);

    long mem_used = 0, mem_total = 0;
    double mem_pct = 0.0;
    get_mem_info(&mem_used, &mem_total, &mem_pct);

    char uptime_str[128];
    get_uptime(uptime_str, sizeof(uptime_str));

    printf("\n");
    printf("%s----------------------------------------------------------------------%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s  System Information Dashboard%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s----------------------------------------------------------------------%s\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sUser / Host:%s       %s%s%s @ %s%s%s\n",
           COLOR_BOLD, COLOR_RESET,
           COLOR_BRIGHT_GREEN, user, COLOR_RESET,
           COLOR_BRIGHT_BLUE, host, COLOR_RESET);
    printf("  %sOS:%s                %s\n", COLOR_BOLD, COLOR_RESET, os_name);
    printf("  %sKernel:%s            %s\n", COLOR_BOLD, COLOR_RESET, kernel_rel);
    printf("  %sArchitecture:%s      %s\n", COLOR_BOLD, COLOR_RESET, arch_str);
    printf("  %sCPU Model:%s         %s (%d cores)\n", COLOR_BOLD, COLOR_RESET, cpu_model, cpu_cores);
    if (mem_total > 0) {
        printf("  %sMemory:%s            %ld MiB / %ld MiB (%.1f%% used)\n",
               COLOR_BOLD, COLOR_RESET, mem_used, mem_total, mem_pct);
    }
    printf("  %sUptime:%s            %s\n", COLOR_BOLD, COLOR_RESET, uptime_str);
    printf("  %sShell:%s             %s %s (Layered Modular)\n",
           COLOR_BOLD, COLOR_RESET, SHELL_NAME, SHELL_VERSION);
    printf("  %sActive Directory:%s  %s\n", COLOR_BOLD, COLOR_RESET, state->current_dir);
    printf("  %sSession History:%s   %d commands recorded\n",
           COLOR_BOLD, COLOR_RESET, state->history_count);
    printf("%s----------------------------------------------------------------------%s\n\n", COLOR_CYAN, COLOR_RESET);

    return 0;
}
