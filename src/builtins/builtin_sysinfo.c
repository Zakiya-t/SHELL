#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtins.h"
#include "ui.h"

#ifndef _WIN32
#include <sys/utsname.h>
#endif

int builtin_sysinfo(int argc, char **argv, ShellState *state)
{
    (void)argc;
    (void)argv;

    const char *user = getenv("USER");
#ifdef _WIN32
    if (!user)
        user = getenv("USERNAME");
#endif
    if (!user)
        user = "developer";

    const char *host = getenv("HOSTNAME");
#ifdef _WIN32
    if (!host)
        host = getenv("COMPUTERNAME");
#endif
    if (!host)
        host = "localhost";

    char os_info[256] = "Linux";
    char arch_info[64] = "x86_64";

#ifdef _WIN32
    strncpy(os_info, "Windows (WSL / Win32 Environment)", sizeof(os_info));
    const char *arch = getenv("PROCESSOR_ARCHITECTURE");
    if (arch)
        strncpy(arch_info, arch, sizeof(arch_info));
#else
    struct utsname un;
    if (uname(&un) == 0) {
        snprintf(os_info, sizeof(os_info), "%s %s", un.sysname, un.release);
        strncpy(arch_info, un.machine, sizeof(arch_info));
    }
#endif

    printf("\n");
    printf("  %s%s     /\\_/\\     %s  %s%s%s%s@%s%s%s%s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET,
           COLOR_BOLD, COLOR_BRIGHT_GREEN, user, COLOR_RESET,
           COLOR_BOLD, COLOR_BRIGHT_BLUE, host, COLOR_RESET);
    printf("  %s%s    ( o.o )    %s  ---------------------------------\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET);
    printf("  %s%s     > ^ <     %s  %sOS:%s        %s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET,
           COLOR_BOLD, COLOR_RESET, os_info);
    printf("  %s%s    /|   |\\    %s  %sHost:%s      %s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET,
           COLOR_BOLD, COLOR_RESET, host);
    printf("  %s%s   (_|   |_)   %s  %sArch:%s      %s\n",
           COLOR_BOLD, COLOR_BRIGHT_CYAN, COLOR_RESET,
           COLOR_BOLD, COLOR_RESET, arch_info);
    printf("                 %sShell:%s     %s %s\n",
           COLOR_BOLD, COLOR_RESET, SHELL_NAME, SHELL_VERSION);
    printf("                 %sDir:%s       %s\n",
           COLOR_BOLD, COLOR_RESET, state->current_dir);
    printf("                 %sHistory:%s   %d commands recorded\n",
           COLOR_BOLD, COLOR_RESET, state->history_count);

    /* Color block palette display (like fastfetch) */
    printf("\n                 ");
    printf("\033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m\n");
    printf("                 ");
    printf("\033[101m   \033[102m   \033[103m   \033[104m   \033[105m   \033[106m   \033[107m   \033[0m\n\n");

    return 0;
}
