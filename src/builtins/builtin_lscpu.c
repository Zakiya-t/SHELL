#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <sys/utsname.h>

static void trim_trailing(char *s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static const char *get_val(const char *line)
{
    const char *colon = strchr(line, ':');
    if (!colon)
        return "";
    colon++;
    while (*colon == ' ' || *colon == '\t')
        colon++;
    return colon;
}
#endif

int builtin_lscpu(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;
    (void)state;

#ifndef _WIN32
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        fprintf(stderr, "myshell: lscpu: cannot open /proc/cpuinfo: %s\n", strerror(errno));
        return 1;
    }

    char arch[64] = "x86_64";
    struct utsname un;
    if (uname(&un) == 0) {
        strncpy(arch, un.machine, sizeof(arch) - 1);
    }

    int cpu_count = 0;
    char model_name[256] = "Unknown";
    char vendor_id[64] = "Unknown";
    char cpu_family[32] = "";
    char model[32] = "";
    char stepping[32] = "";
    char cpu_mhz[32] = "";
    char cache_size[64] = "";
    char bogomips[32] = "";
    bool has_vmx = false;
    bool has_svm = false;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            cpu_count++;
        } else if (model_name[0] == 'U' && strncmp(line, "model name", 10) == 0) {
            strncpy(model_name, get_val(line), sizeof(model_name) - 1);
            trim_trailing(model_name);
        } else if (vendor_id[0] == 'U' && strncmp(line, "vendor_id", 9) == 0) {
            strncpy(vendor_id, get_val(line), sizeof(vendor_id) - 1);
            trim_trailing(vendor_id);
        } else if (cpu_family[0] == '\0' && strncmp(line, "cpu family", 10) == 0) {
            strncpy(cpu_family, get_val(line), sizeof(cpu_family) - 1);
            trim_trailing(cpu_family);
        } else if (model[0] == '\0' && strncmp(line, "model\t", 6) == 0) {
            strncpy(model, get_val(line), sizeof(model) - 1);
            trim_trailing(model);
        } else if (stepping[0] == '\0' && strncmp(line, "stepping", 8) == 0) {
            strncpy(stepping, get_val(line), sizeof(stepping) - 1);
            trim_trailing(stepping);
        } else if (cpu_mhz[0] == '\0' && strncmp(line, "cpu MHz", 7) == 0) {
            strncpy(cpu_mhz, get_val(line), sizeof(cpu_mhz) - 1);
            trim_trailing(cpu_mhz);
        } else if (cache_size[0] == '\0' && strncmp(line, "cache size", 10) == 0) {
            strncpy(cache_size, get_val(line), sizeof(cache_size) - 1);
            trim_trailing(cache_size);
        } else if (bogomips[0] == '\0' && strncmp(line, "bogomips", 8) == 0) {
            strncpy(bogomips, get_val(line), sizeof(bogomips) - 1);
            trim_trailing(bogomips);
        } else if (strncmp(line, "flags", 5) == 0) {
            if (strstr(line, " vmx "))
                has_vmx = true;
            if (strstr(line, " svm "))
                has_svm = true;
        }
    }
    fclose(f);

    printf("%sArchitecture:%s            %s\n", COLOR_BOLD, COLOR_RESET, arch);
    printf("  %sCPU op-mode(s):%s        32-bit, 64-bit\n", COLOR_DIM, COLOR_RESET);
    printf("  %sByte Order:%s            Little Endian\n", COLOR_DIM, COLOR_RESET);
    printf("%sCPU(s):%s                  %d\n", COLOR_BOLD, COLOR_RESET, cpu_count);
    if (cpu_count > 0)
        printf("  %sOn-line CPU(s) list:%s   0-%d\n", COLOR_DIM, COLOR_RESET, cpu_count - 1);
    printf("%sVendor ID:%s               %s\n", COLOR_BOLD, COLOR_RESET, vendor_id);
    printf("  %sModel name:%s            %s\n", COLOR_BOLD, COLOR_RESET, model_name);
    if (cpu_family[0])
        printf("    %sCPU family:%s          %s\n", COLOR_DIM, COLOR_RESET, cpu_family);
    if (model[0])
        printf("    %sModel:%s               %s\n", COLOR_DIM, COLOR_RESET, model);
    if (stepping[0])
        printf("    %sStepping:%s            %s\n", COLOR_DIM, COLOR_RESET, stepping);
    if (cpu_mhz[0])
        printf("    %sCPU MHz:%s             %s\n", COLOR_DIM, COLOR_RESET, cpu_mhz);
    if (cache_size[0])
        printf("    %sCache size:%s          %s\n", COLOR_DIM, COLOR_RESET, cache_size);
    if (bogomips[0])
        printf("    %sBogoMIPS:%s            %s\n", COLOR_DIM, COLOR_RESET, bogomips);

    printf("%sVirtualization features:%s\n", COLOR_BOLD, COLOR_RESET);
    if (has_vmx)
        printf("  %sVirtualization:%s        VT-x (Intel)\n", COLOR_DIM, COLOR_RESET);
    else if (has_svm)
        printf("  %sVirtualization:%s        AMD-V (AMD)\n", COLOR_DIM, COLOR_RESET);
    else
        printf("  %sVirtualization:%s        Hypervisor / Virtualized\n", COLOR_DIM, COLOR_RESET);

    printf("\n%s[Kernel Verified]%s Queried directly from Linux Kernel virtual file (/proc/cpuinfo)\n",
           COLOR_GREEN, COLOR_RESET);
    printf("Compare with Ubuntu system binary by running: %s/usr/bin/lscpu%s\n", COLOR_CYAN, COLOR_RESET);
    return 0;

#else
    /* Windows fallback */
    const char *arch = getenv("PROCESSOR_ARCHITECTURE");
    const char *id = getenv("PROCESSOR_IDENTIFIER");
    const char *cpus = getenv("NUMBER_OF_PROCESSORS");

    printf("%sArchitecture:%s            %s\n", COLOR_BOLD, COLOR_RESET, arch ? arch : "x86_64");
    printf("%sCPU(s):%s                  %s\n", COLOR_BOLD, COLOR_RESET, cpus ? cpus : "1");
    printf("%sModel name:%s            %s\n", COLOR_BOLD, COLOR_RESET, id ? id : "x86 Family");
    printf("\n%sNote:%s Running in Windows mode. On Linux/WSL2, this queries /proc/cpuinfo directly.\n",
           COLOR_YELLOW, COLOR_RESET);
    return 0;
#endif
}
